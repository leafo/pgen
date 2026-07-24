-- Target-independent helpers shared by the code generators (C and Lua):
-- grammar collection passes, rule extraction, and template utilities.
local common = {}
local types = require("pgen.types")

-- Replace $VARNAME$ placeholders in a template with values from vars.
-- Unknown placeholders are left untouched.
function common.template_code(template, vars)
  local result = template:gsub("%$([A-Z_]+)%$", function(var_name)
    local value = vars[var_name]
    if value ~= nil then
      return tostring(value)
    else
      return "$" .. var_name .. "$"
    end
  end)
  return result
end

-- Iterator for rules, sorted with start_rule first, then alphabetically
function common.sorted_rules(rules, start_rule)
  local names = {}
  for name in pairs(rules) do
    if name ~= start_rule then
      table.insert(names, name)
    end
  end
  table.sort(names)
  table.insert(names, 1, start_rule)
  local i = 0
  return function()
    i = i + 1
    local name = names[i]
    if name then
      return name, rules[name]
    end
  end
end

-- Split a grammar table into its rules and the start rule (either the rule
-- named by grammar[1] or the first rule encountered)
function common.extract_rules(grammar)
  local rules = {}
  local start_rule = nil

  for name, pattern in pairs(grammar) do
    local skip = false
    if not start_rule then
      if type(pattern) == "string" and name == 1 then
        start_rule = pattern
        skip = true
      else
        start_rule = name
      end
    end
    if not skip then
      rules[name] = pattern
    end
  end

  if not start_rule then
    error("Grammar does not contain a starting rule")
  end

  return rules, start_rule
end

-- Collect all Cg and Cmb names from a grammar (both use sentinels)
function common.collect_cg_names(grammar)
  local visitor = require("pgen.visitor")
  local names = {}
  visitor.visit_grammar(grammar, function(node)
    if node.type == types.Cg or node.type == types.Cmb then
      names[node.name] = true
    end
  end)
  -- Convert to sorted array for deterministic output
  local result = {}
  for name in pairs(names) do
    table.insert(result, name)
  end
  table.sort(result)
  return result
end

-- Collect all unique non-nil values from Cc nodes in a grammar. These are
-- interned into the Lua registry once at module load; capture-log CONST
-- entries reference them by registry ref, so matching never constructs
-- constant values. Returns a deterministically ordered array of values and
-- a value -> 0-based index map.
function common.collect_constants(grammar)
  local visitor = require("pgen.visitor")
  local seen = {}
  visitor.visit_grammar(grammar, function(node)
    if node.type == types.Cc then
      local values = node.value
      for i = 1, values.count do
        if values[i] ~= nil then
          seen[values[i]] = true
        end
      end
    end
  end)

  local strings, numbers = {}, {}
  local has_false, has_true = false, false
  for value in pairs(seen) do
    local t = type(value)
    if t == "string" then
      table.insert(strings, value)
    elseif t == "number" then
      table.insert(numbers, value)
    elseif t == "boolean" then
      if value then has_true = true else has_false = true end
    end
  end
  table.sort(strings)
  table.sort(numbers)

  local pool, index = {}, {}
  local function add(value)
    table.insert(pool, value)
    index[value] = #pool - 1
  end
  for _, value in ipairs(strings) do add(value) end
  for _, value in ipairs(numbers) do add(value) end
  if has_false then add(false) end
  if has_true then add(true) end

  return pool, index
end

-- Collect all Cmt and Cfn nodes from a grammar, assigning each a unique ID
-- into a shared callback registry. Returns array of {id, code, kind} for
-- unique codes, and a new grammar with cmt_id fields set. Identical code
-- strings of the same kind share an ID; the kinds are registered separately
-- because their load conventions differ (a Cmt chunk IS the callback, while
-- a Cfn chunk is run once and must return the callback).
function common.collect_cmt_codes(grammar)
  local visitor = require("pgen.visitor")
  local codes = {}           -- Array of {id, code, kind} for unique codes only
  local code_to_id = {}      -- Map: kind-tagged code string -> id (deduplication)
  local next_id = 0

  local new_grammar = visitor.visit_grammar(grammar, function(node, replace)
    if (node.type == types.Cmt or node.type == types.Cfn) and node.cmt_id == nil then
      local kind = node.type == types.Cmt and "cmt" or "cfn"
      local code = node.code
      local key = kind .. "\0" .. code
      local id = code_to_id[key]

      if id == nil then
        -- First time seeing this code, assign new ID
        id = next_id
        code_to_id[key] = id
        table.insert(codes, {id = id, code = code, kind = kind})
        next_id = next_id + 1
      end

      -- Create a copy of the node with cmt_id assigned (possibly shared)
      replace(visitor.copy_node(node, {cmt_id = id}))
    end
  end)

  return codes, new_grammar
end

-- Collect all indenter descriptors referenced by Ind nodes in a grammar,
-- assigning each distinct descriptor (by table identity) a stack id.
-- Rules are visited in sorted order so ids are deterministic regardless of
-- table iteration order.
-- Returns array of {id, tab_width, initial} and a new grammar with stack_id
-- set on every Ind node.
function common.collect_indenters(grammar)
  local visitor = require("pgen.visitor")
  local descriptors = {}
  local desc_to_id = {}

  local names = {}
  for name in pairs(grammar) do
    table.insert(names, name)
  end
  table.sort(names, function(a, b)
    return tostring(a) < tostring(b)
  end)

  local new_grammar = {}
  local changed = false

  for _, name in ipairs(names) do
    local pattern = grammar[name]
    if type(pattern) == "table" then
      local new_pattern = visitor.visit_pattern(pattern, function(node, replace)
        if node.type == types.Ind then
          local desc = node.indenter
          if type(desc) ~= "table" then
            error("Ind node is missing its indenter descriptor")
          end
          local id = desc_to_id[desc]
          if id == nil then
            id = #descriptors
            desc_to_id[desc] = id
            table.insert(descriptors, {
              id = id,
              tab_width = desc.tab_width or 4,
              initial = desc.initial or 0
            })
          end
          if node.stack_id ~= id then
            replace(visitor.copy_node(node, {stack_id = id}))
          end
        end
      end)
      new_grammar[name] = new_pattern
      if new_pattern ~= pattern then changed = true end
    else
      new_grammar[name] = pattern
    end
  end

  if #descriptors > 256 then
    error("Too many indenter stacks in grammar (max 256)")
  end

  return descriptors, changed and new_grammar or grammar
end

return common

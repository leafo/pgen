local visitor = {}
local types = require("pgen.types")

-- Visit every node in a pattern tree, calling visitor on each
-- visitor(node, replace) - call replace(new_node) to replace current node
-- Returns the (possibly replaced) pattern
function visitor.visit_pattern(pattern, visitor_fn)
  if not pattern or type(pattern) ~= "table" then
    return pattern
  end

  -- Allow visitor to replace this node
  local replacement = nil
  local function replace(new_node)
    replacement = new_node
  end

  visitor_fn(pattern, replace)

  -- If replaced, visit the replacement instead
  if replacement then
    return visitor.visit_pattern(replacement, visitor_fn)
  end

  -- Visit children and rebuild if any changed
  local t = pattern.type
  if t == types.C or t == types.Ct or t == types.L or t == types.Cg or t == types.Cn or t == types.Cmt then
    local new_value = visitor.visit_pattern(pattern.value, visitor_fn)
    if new_value ~= pattern.value then
      return setmetatable({type = t, value = new_value, name = pattern.name, code = pattern.code}, getmetatable(pattern))
    end
  elseif t == "sequence" or t == "choice" then
    local changed = false
    local new_children = {}
    for i, child in ipairs(pattern) do
      local new_child = visitor.visit_pattern(child, visitor_fn)
      new_children[i] = new_child
      if new_child ~= child then changed = true end
    end
    if changed then
      local new_pattern = setmetatable({type = t}, getmetatable(pattern))
      for i, child in ipairs(new_children) do
        new_pattern[i] = child
      end
      return new_pattern
    end
  elseif t == "repeat" or t == "negate" then
    local new_child = visitor.visit_pattern(pattern[1], visitor_fn)
    if new_child ~= pattern[1] then
      return setmetatable({type = t, new_child, pattern[2]}, getmetatable(pattern))
    end
  end

  return pattern
end

-- Visit every node across all rules in a grammar
-- Returns new grammar with any replacements applied
function visitor.visit_grammar(grammar, visitor_fn)
  local new_grammar = {}
  local changed = false

  for name, pattern in pairs(grammar) do
    if type(pattern) == "table" then
      local new_pattern = visitor.visit_pattern(pattern, visitor_fn)
      new_grammar[name] = new_pattern
      if new_pattern ~= pattern then changed = true end
    else
      new_grammar[name] = pattern
    end
  end

  return changed and new_grammar or grammar
end

return visitor

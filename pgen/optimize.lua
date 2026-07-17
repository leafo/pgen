local optimize = {}
local types = require("pgen.types")

-- Flatten nested choices: choice(choice(a,b), c) → {a, b, c}
local function flatten_choices(pattern)
  if pattern.type ~= "choice" then
    return {pattern}
  end

  local result = {}
  for _, child in ipairs(pattern) do
    local flattened = flatten_choices(child)
    for _, alt in ipairs(flattened) do
      table.insert(result, alt)
    end
  end
  return result
end

-- Check if ALL alternatives are string literals (P"..." nodes only) and
-- ordering won't change semantics when trie prefers longest prefix matches
local function is_trie_eligible(alternatives)
  if #alternatives < 3 then
    return false
  end

  local strings = {}
  for i, alt in ipairs(alternatives) do
    -- Must be P node (type == types.P) with string value
    if alt.type ~= types.P then return false end
    if type(alt.value) ~= "string" then return false end
    if #alt.value == 0 then return false end
    strings[i] = alt.value
  end

  -- Only require descending order for items that share prefixes.
  for i = 1, #strings do
    local a = strings[i]
    for j = 1, #strings do
      if i ~= j then
        local b = strings[j]
        if #a < #b and b:sub(1, #a) == a then
          -- a is a prefix of b, b must appear before a
          if j > i then
            return false
          end
        end
      end
    end
  end

  return true
end

local function contains_cg_in_pattern(pattern, rules, memo, visiting)
  local visitor = require("pgen.visitor")

  if not pattern or type(pattern) ~= "table" then
    return false
  end

  local found = false

  visitor.visit_pattern(pattern, function(node)
    if node.type == types.Cg then
      found = true
      return visitor.STOP
    end

    if node.type == types.V then
      local name = node.value
      if memo[name] ~= nil then
        if memo[name] then
          found = true
          return visitor.STOP
        end
        return
      end
      if visiting[name] then
        -- Conservative: recursion could contain Cg in other branch
        found = true
        return visitor.STOP
      end

      local rule = rules[name]
      if type(rule) ~= "table" then
        -- Unknown rule, avoid optimizing
        found = true
        return visitor.STOP
      end

      visiting[name] = true
      if contains_cg_in_pattern(rule, rules, memo, visiting) then
        found = true
      end
      visiting[name] = nil
      memo[name] = found

      if found then
        return visitor.STOP
      end
    end
  end)

  return found
end

-- Mark Ct nodes that don't contain any Cg nodes
function optimize.capture_table_optimization(grammar)
  local pgen = require("pgen")
  local visitor = require("pgen.visitor")
  local memo = {}
  local visiting = {}

  return visitor.visit_grammar(grammar, function(node, replace)
    if node.type ~= types.Ct then return end
    if contains_cg_in_pattern(node.value, grammar, memo, visiting) then return end
    replace(visitor.copy_node(node, {array_only = true}))
  end)
end

-- Build trie data structure from list of strings
-- Returns: {children = {char -> node}, is_terminal = bool, word = string|nil}
local function build_trie(strings)
  local root = {children = {}, is_terminal = false}

  for _, str in ipairs(strings) do
    local node = root
    for i = 1, #str do
      local char = str:sub(i, i)
      if not node.children[char] then
        node.children[char] = {children = {}, is_terminal = false}
      end
      node = node.children[char]
    end
    node.is_terminal = true
    node.word = str
  end

  return root
end

-- Create trie AST node from list of strings
local function create_trie_node(strings)
  local pgen = require("pgen")
  return pgen._make({
    type = "literal_trie",
    trie = build_trie(strings),
    strings = strings  -- keep for debugging/comments
  })
end

-- Trie optimization pass - replaces choice of literals with trie node
function optimize.trie_optimization(grammar)
  local pgen = require("pgen")
  local visitor = require("pgen.visitor")
  return visitor.visit_grammar(grammar, function(node, replace)
    if node.type ~= "choice" then return end

    local alternatives = flatten_choices(node)
    if not is_trie_eligible(alternatives) then return end

    -- All alternatives are string literals - create trie node
    local strings = {}
    for _, alt in ipairs(alternatives) do
      table.insert(strings, alt.value)
    end

    replace(create_trie_node(strings))
  end)
end

-- Replace a sufficiently wide ordered choice with a byte dispatcher. The
-- dispatcher only removes alternatives whose FIRST set proves they cannot
-- match; all remaining alternatives retain their original PEG order.
function optimize.dispatch_choice_optimization(grammar)
  local pgen = require("pgen")
  local visitor = require("pgen.visitor")
  local analyze = require("pgen.analyze")
  local nullable_memo = {}
  local rule_first = analyze.first_sets(grammar, nullable_memo)

  -- Pattern nodes are immutable, so summaries can be cached per node: the
  -- alternatives of a rejected outer choice come up again as the visitor
  -- descends into the nested binary choice tails.
  local summary_cache = {}
  local function alternative_summary(alternative)
    local summary = summary_cache[alternative]
    if not summary then
      summary = analyze.first_set(alternative, grammar, rule_first, nullable_memo)
      -- An alternative that may match empty, or whose beginning is unknown,
      -- must stay a candidate for every byte and at end of input. So must a
      -- known non-nullable alternative with an empty FIRST set: it can never
      -- succeed (e.g. it enters left recursion), and eliminating it would
      -- change how the parser fails, hiding the recursion-depth error the
      -- unoptimized parser reports.
      summary.always = summary.unknown or
        next(summary.bytes) == nil or
        analyze.is_nullable(alternative, grammar, nullable_memo, {})
      summary_cache[alternative] = summary
    end
    return summary
  end

  return visitor.visit_grammar(grammar, function(node, replace)
    if node.type ~= "choice" then return end

    local alternatives = flatten_choices(node)
    -- A uint64_t candidate mask keeps generated code compact. Small choices
    -- are cheaper as ordinary branches and need no dispatch scaffolding.
    if #alternatives < 4 or #alternatives > 64 then return end

    local summaries = {}
    for i, alternative in ipairs(alternatives) do
      summaries[i] = alternative_summary(alternative)
    end

    -- Count candidates per byte before materializing candidate lists so
    -- rejected choices (the common case) allocate nothing.
    local always_count = 0
    local byte_counts = {}
    for _, summary in ipairs(summaries) do
      if summary.always then
        always_count = always_count + 1
      else
        for byte in pairs(summary.bytes) do
          byte_counts[byte] = (byte_counts[byte] or 0) + 1
        end
      end
    end

    local min_byte_count = math.huge
    for byte = 0, 255 do
      local count = byte_counts[byte] or 0
      if count < min_byte_count then min_byte_count = count end
    end

    -- Requiring at least two eliminated alternatives amortizes the switch
    -- and avoids rewriting choices that are effectively undispatchable.
    if #alternatives - (always_count + min_byte_count) < 2 then return end

    local byte_candidates = {}
    for byte = 0, 255 do
      local candidates = {}
      for i, summary in ipairs(summaries) do
        if summary.always or summary.bytes[byte] then
          candidates[#candidates + 1] = i
        end
      end
      byte_candidates[byte] = candidates
    end

    local eof_candidates = {}
    for i, summary in ipairs(summaries) do
      if summary.always then
        eof_candidates[#eof_candidates + 1] = i
      end
    end

    -- Alternatives live in the array part like sequence/choice children, so
    -- generic traversals (visitor, analyses) need no special child accessor.
    local dispatch = {
      type = "dispatch_choice",
      byte_candidates = byte_candidates,
      eof_candidates = eof_candidates,
    }
    for i, alternative in ipairs(alternatives) do
      dispatch[i] = alternative
    end
    replace(pgen._make(dispatch))
    -- The visitor's default replace behavior descends into the replacement's
    -- children, so wide choices nested inside the alternatives are dispatched
    -- too. Replacing a nested node rebuilds this one index-aligned, keeping
    -- the candidate masks valid.
  end)
end

-- Main entry point: apply all optimization passes
function optimize.optimize_grammar(grammar)
  grammar = optimize.trie_optimization(grammar)
  grammar = optimize.dispatch_choice_optimization(grammar)
  grammar = optimize.capture_table_optimization(grammar)
  -- Future: add more optimization passes here
  return grammar
end

return optimize

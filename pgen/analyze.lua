local analyze = {}
local types = require("pgen.types")

-- Determine whether matching a pattern may change state that must be rolled
-- back with the input position (captures pushed onto the Lua stack, indenter
-- stack operations). Rule references are resolved through rule_states, a
-- precomputed name -> boolean table from analyze.stateful_rules; rules
-- missing from the grammar are treated as stateful.
function analyze.changes_backtrack_state(pattern, rules, rule_states)
  if type(pattern) ~= "table" then
    return true
  end

  rules = rules or {}
  rule_states = rule_states or {}

  local t = pattern.type

  if t == types.C or t == types.Ct or t == types.Cp or t == types.Cc or
      t == types.Cg or t == types.Cn or t == types.Cmt or t == types.Cfn or
      t == types.Ind then
    return true
  elseif t == types.P or t == types.R or t == types.S or t == types.Cmb or
      t == types.T or t == "literal_trie" then
    return false
  elseif t == types.V then
    local name = pattern.value
    if type(rules[name]) ~= "table" then
      return true
    end
    return rule_states[name] or false
  elseif t == types.L then
    return analyze.changes_backtrack_state(pattern.value, rules, rule_states)
  elseif t == "repeat" or t == "negate" then
    return analyze.changes_backtrack_state(pattern[1], rules, rule_states)
  elseif t == "sequence" or t == "choice" then
    for _, child in ipairs(pattern) do
      if analyze.changes_backtrack_state(child, rules, rule_states) then
        return true
      end
    end
    return false
  end

  -- New pattern types must opt into the capture-free fast path explicitly.
  return true
end

-- Compute changes_backtrack_state for every rule in the grammar as a least
-- fixed point: rules start assumed state-free and are flipped to stateful
-- until stable, so cycles of state-free rules correctly resolve to false.
function analyze.stateful_rules(rules)
  local states = {}
  local changed = true
  while changed do
    changed = false
    for name, pattern in pairs(rules) do
      if not states[name] and
          analyze.changes_backtrack_state(pattern, rules, states) then
        states[name] = true
        changed = true
      end
    end
  end
  return states
end

-- Determine whether a pattern can succeed without consuming any input.
-- Conservative: returns true when uncertain (recursive or unknown rules), so
-- a `true` result means "may match empty", never "definitely matches empty".
--
-- rules: the grammar's rule table, for resolving V references
-- rule_memo: memoized results per rule name
-- visiting: rule names currently on the resolution stack (cycle detection)
function analyze.is_nullable(pattern, rules, rule_memo, visiting)
  if type(pattern) ~= "table" then
    return true
  end

  local t = pattern.type

  if t == types.P then
    local v = pattern.value
    if type(v) == "string" then
      return #v == 0
    else
      -- P(n) matches exactly n characters
      return v == 0
    end
  elseif t == types.R or t == types.S then
    return false -- always consume one character
  elseif t == types.T then
    return false -- never succeeds
  elseif t == types.Cp or t == types.Cc or t == types.Ind then
    return true -- consume nothing
  elseif t == types.Cmb then
    return true -- the referenced capture may be the empty string
  elseif t == types.L then
    return true -- lookahead consumes nothing
  elseif t == types.C or t == types.Ct or t == types.Cg or t == types.Cn or
      t == types.Cfn then
    return analyze.is_nullable(pattern.value, rules, rule_memo, visiting)
  elseif t == types.Cmt then
    -- the callback can only advance past the inner match, so an empty match
    -- requires the inner pattern to match empty
    return analyze.is_nullable(pattern.value, rules, rule_memo, visiting)
  elseif t == types.V then
    local name = pattern.value
    if rule_memo[name] ~= nil then
      return rule_memo[name]
    end
    if visiting[name] then
      -- recursive reference: conservatively nullable
      return true
    end
    local rule = rules[name]
    if type(rule) ~= "table" then
      -- unknown rule: conservative
      return true
    end
    visiting[name] = true
    local result = analyze.is_nullable(rule, rules, rule_memo, visiting)
    visiting[name] = nil
    rule_memo[name] = result
    return result
  elseif t == "sequence" then
    for _, child in ipairs(pattern) do
      if not analyze.is_nullable(child, rules, rule_memo, visiting) then
        return false
      end
    end
    return true
  elseif t == "choice" then
    for _, child in ipairs(pattern) do
      if analyze.is_nullable(child, rules, rule_memo, visiting) then
        return true
      end
    end
    return false
  elseif t == "negate" then
    return true -- predicate consumes nothing
  elseif t == "repeat" then
    local n = pattern[2]
    if n <= 0 then
      -- a^0 matches empty, a^-n matches empty
      return true
    end
    return analyze.is_nullable(pattern[1], rules, rule_memo, visiting)
  elseif t == "literal_trie" then
    return false -- tries are only built from non-empty literals
  else
    -- unknown pattern type: conservative
    return true
  end
end

-- Error if any unbounded repetition (patt^n for n >= 0) has a body that may
-- match the empty string: such a loop never advances and hangs the parser.
-- Equivalent to LPeg's "loop body may accept empty string" compile error.
function analyze.check_loops(rules)
  local visitor = require("pgen.visitor")
  local rule_memo = {}

  -- check rules in sorted order for deterministic error messages
  local names = {}
  for name in pairs(rules) do
    table.insert(names, name)
  end
  table.sort(names, function(a, b)
    return tostring(a) < tostring(b)
  end)

  for _, name in ipairs(names) do
    local pattern = rules[name]
    if type(pattern) == "table" then
      visitor.visit_pattern(pattern, function(node)
        if node.type == "repeat" and node[2] >= 0 then
          if analyze.is_nullable(node[1], rules, rule_memo, {}) then
            error(("Rule '%s': loop body may accept empty string (would loop forever)"):format(tostring(name)), 0)
          end
        end
      end)
    end
  end
end

return analyze

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
  elseif t == "sequence" or t == "choice" or t == "dispatch_choice" then
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

-- Determine whether a pattern's outcome depends only on the input position.
-- Stricter than changes_backtrack_state: besides captures and indenter
-- state, it excludes labeled failures (T adds a third outcome besides
-- success/failure) and backreferences (Cmb reads the capture log). A pure
-- pattern always produces the same success/end-position result at a given
-- position, which makes it safe to memoize. rule_purity is a precomputed
-- name -> boolean table from analyze.pure_rules.
function analyze.position_pure(pattern, rules, rule_purity)
  if type(pattern) ~= "table" then
    return false
  end

  local t = pattern.type

  if t == types.P or t == types.R or t == types.S or t == "literal_trie" then
    return true
  elseif t == types.V then
    local name = pattern.value
    if type(rules[name]) ~= "table" then
      return false
    end
    return rule_purity[name] or false
  elseif t == types.L then
    return analyze.position_pure(pattern.value, rules, rule_purity)
  elseif t == "repeat" or t == "negate" then
    return analyze.position_pure(pattern[1], rules, rule_purity)
  elseif t == "sequence" or t == "choice" or t == "dispatch_choice" then
    for _, child in ipairs(pattern) do
      if not analyze.position_pure(child, rules, rule_purity) then
        return false
      end
    end
    return true
  end

  -- captures, Cmt/Cfn, indenter ops, T, Cmb, and unknown types
  return false
end

-- Compute position purity for every rule as a greatest fixed point: rules
-- start assumed pure and are flipped until stable, so cycles of pure rules
-- correctly stay pure.
function analyze.pure_rules(rules)
  local purity = {}
  for name in pairs(rules) do
    purity[name] = true
  end
  local changed = true
  while changed do
    changed = false
    for name, pattern in pairs(rules) do
      if purity[name] and not analyze.position_pure(pattern, rules, purity) then
        purity[name] = false
        changed = true
      end
    end
  end
  return purity
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
      -- P(n) matches exactly n characters; P(-n) asserts fewer than n
      -- characters remain and consumes nothing when it succeeds
      return v <= 0
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
  elseif t == "choice" or t == "dispatch_choice" then
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

local function add_bytes(dst, src)
  for byte in pairs(src) do
    dst[byte] = true
  end
end

-- Return the set of bytes that may begin a match and whether the pattern's
-- beginning is unknown. Unknown patterns must remain candidates for every
-- dispatch byte. Whether a pattern may succeed without consuming input is
-- not part of the summary: that is is_nullable's job, and nullable_memo is
-- its per-grammar rule memo. The rule_first table is computed to a fixed
-- point by first_sets below.
function analyze.first_set(pattern, rules, rule_first, nullable_memo)
  if type(pattern) ~= "table" then
    return {bytes = {}, unknown = true}
  end

  local t = pattern.type
  local result = {bytes = {}, unknown = false}

  if t == types.P then
    local value = pattern.value
    if type(value) == "string" then
      if #value > 0 then
        result.bytes[value:byte(1)] = true
      end
    elseif value > 0 then
      for byte = 0, 255 do result.bytes[byte] = true end
    end
    -- P"" and P(n <= 0) consume nothing; is_nullable accounts for them
  elseif t == types.R then
    for _, range in ipairs(pattern.value) do
      local low, high = range:byte(1, 2)
      for byte = low, high do result.bytes[byte] = true end
    end
  elseif t == types.S then
    for i = 1, #pattern.value do
      result.bytes[pattern.value:byte(i)] = true
    end
  elseif t == "literal_trie" then
    for char in pairs(pattern.trie.children) do
      result.bytes[char:byte(1)] = true
    end
  elseif t == types.V then
    local summary = rule_first[pattern.value]
    if type(rules[pattern.value]) ~= "table" or not summary then
      result.unknown = true
    else
      add_bytes(result.bytes, summary.bytes)
      result.unknown = summary.unknown
    end
  elseif t == types.C or t == types.Ct or t == types.Cg or t == types.Cn or
      t == types.Cfn then
    return analyze.first_set(pattern.value, rules, rule_first, nullable_memo)
  elseif t == types.Cp or t == types.Cc then
    -- consume nothing; contribute no bytes
  elseif t == "sequence" then
    for _, child in ipairs(pattern) do
      local child_first = analyze.first_set(child, rules, rule_first, nullable_memo)
      add_bytes(result.bytes, child_first.bytes)
      result.unknown = result.unknown or child_first.unknown
      if not analyze.is_nullable(child, rules, nullable_memo, {}) then
        break
      end
    end
  elseif t == "choice" or t == "dispatch_choice" then
    for _, child in ipairs(pattern) do
      local child_first = analyze.first_set(child, rules, rule_first, nullable_memo)
      add_bytes(result.bytes, child_first.bytes)
      result.unknown = result.unknown or child_first.unknown
    end
  elseif t == "repeat" then
    local child_first = analyze.first_set(pattern[1], rules, rule_first, nullable_memo)
    add_bytes(result.bytes, child_first.bytes)
    result.unknown = child_first.unknown
  elseif t == types.L or t == "negate" then
    -- Predicates consume nothing, but their success depends on input. Keeping
    -- them unknown prevents dispatch from skipping observable failures.
    result.unknown = true
  else
    -- Cmt, Cmb, T, indenter operations, and future pattern types may inspect
    -- state or fail before a terminal is reached.
    result.unknown = true
  end

  return result
end

-- Compute FIRST-byte summaries for all rules as a monotone fixed point. This
-- handles recursive expression grammars without treating every recursive
-- reference as unknown. nullable_memo is optional and lets callers share
-- is_nullable's rule memo with their own queries.
function analyze.first_sets(rules, nullable_memo)
  nullable_memo = nullable_memo or {}

  local summaries = {}
  for name, pattern in pairs(rules) do
    if type(name) == "string" and type(pattern) == "table" then
      summaries[name] = {bytes = {}, unknown = false}
    end
  end

  -- first_set is monotone in rule_first and every summary starts at bottom,
  -- so a recomputed summary can only gain bytes or flip unknown on.
  local function differs(old, new)
    if old.unknown ~= new.unknown then return true end
    for byte in pairs(new.bytes) do
      if not old.bytes[byte] then return true end
    end
    return false
  end

  local changed = true
  while changed do
    changed = false
    for name, old in pairs(summaries) do
      local new = analyze.first_set(rules[name], rules, summaries, nullable_memo)
      if differs(old, new) then
        summaries[name] = new
        changed = true
      end
    end
  end

  return summaries
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

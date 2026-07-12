local visitor = {}
local types = require("pgen.types")

-- Control flow constants for visitor callbacks
visitor.STOP = "stop"           -- Abort entire scan immediately
visitor.SKIP_CHILDREN = "skip"  -- Don't recurse into this node's children

function visitor.copy_node(node, overrides)
  local new_node = setmetatable({}, getmetatable(node))
  for k, v in pairs(node) do
    new_node[k] = v
  end
  if overrides then
    for k, v in pairs(overrides) do
      new_node[k] = v
    end
  end
  return new_node
end

-- Visit every node in a pattern tree, calling visitor on each
-- visitor(node, replace) - call replace(new_node) to replace current node
-- Callback can return visitor.STOP to abort scan, or visitor.SKIP_CHILDREN to skip recursion
--
-- Replacement behavior:
--   - By default, only the replacement's children are visited (not the replacement itself)
--   - STOP + replace(): replacement is inserted into result, children not visited, scan aborts
--   - SKIP_CHILDREN + replace(): replacement is inserted into result, children not visited
--
-- Returns: pattern, stopped (stopped is true if STOP was returned)
function visitor.visit_pattern(pattern, visitor_fn)
  if not pattern or type(pattern) ~= "table" then
    return pattern, false
  end

  -- Allow visitor to replace this node
  local replacement = nil
  local function replace(new_node)
    replacement = new_node
  end

  local result = visitor_fn(pattern, replace)

  -- Handle STOP - abort entire scan (replacement kept if provided)
  if result == visitor.STOP then
    return replacement or pattern, true
  end

  -- Handle SKIP_CHILDREN - use replacement if any, but don't recurse
  if result == visitor.SKIP_CHILDREN then
    return replacement or pattern, false
  end

  -- If replaced, visit only the replacement's children (not the replacement itself)
  if replacement then
    pattern = replacement
    -- Fall through to child traversal below
  end

  -- Visit children and rebuild if any changed
  -- Leaf types (P, R, S, V, Cp, Cc, Cmb, T, Ind) have no child patterns and
  -- need no traversal case here
  local t = pattern.type
  if t == types.C or t == types.Ct or t == types.L or t == types.Cg or t == types.Cn or t == types.Cmt or t == types.Cfn then
    local new_value, stopped = visitor.visit_pattern(pattern.value, visitor_fn)
    if stopped then
      return pattern, true
    end
    if new_value ~= pattern.value then
      return visitor.copy_node(pattern, {value = new_value}), false
    end
  elseif t == "sequence" or t == "choice" then
    local changed = false
    local new_children = {}
    for i, child in ipairs(pattern) do
      local new_child, stopped = visitor.visit_pattern(child, visitor_fn)
      if stopped then
        return pattern, true
      end
      new_children[i] = new_child
      if new_child ~= child then changed = true end
    end
    if changed then
      local new_pattern = visitor.copy_node(pattern)
      for i, child in ipairs(new_children) do
        new_pattern[i] = child
      end
      return new_pattern, false
    end
  elseif t == "repeat" or t == "negate" then
    local new_child, stopped = visitor.visit_pattern(pattern[1], visitor_fn)
    if stopped then
      return pattern, true
    end
    if new_child ~= pattern[1] then
      return visitor.copy_node(pattern, {[1] = new_child}), false
    end
  end

  return pattern, false
end

-- Visit every node across all rules in a grammar
-- Returns: grammar, stopped (stopped is true if STOP was returned)
function visitor.visit_grammar(grammar, visitor_fn)
  local new_grammar = {}
  local changed = false

  for name, pattern in pairs(grammar) do
    if type(pattern) == "table" then
      local new_pattern, stopped = visitor.visit_pattern(pattern, visitor_fn)
      if stopped then
        return grammar, true
      end
      new_grammar[name] = new_pattern
      if new_pattern ~= pattern then changed = true end
    else
      new_grammar[name] = pattern
    end
  end

  return changed and new_grammar or grammar, false
end

return visitor

local optimize = {}

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
    -- Must be P node (type == 1) with string value
    if alt.type ~= 1 then return false end
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

-- Build trie data structure from list of strings
-- Returns: {children = {char -> node}, is_terminal = bool, word = string|nil}
function optimize.build_trie(strings)
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
function optimize.create_trie_node(strings)
  local pgen = require("pgen")
  return pgen._make({
    type = "literal_trie",
    trie = optimize.build_trie(strings),
    strings = strings  -- keep for debugging/comments
  })
end

-- Trie optimization pass - replaces choice of literals with trie node
function optimize.trie_optimization(grammar)
  local pgen = require("pgen")

  return pgen.visit_grammar(grammar, function(node, replace)
    if node.type ~= "choice" then return end

    local alternatives = flatten_choices(node)
    if not is_trie_eligible(alternatives) then return end

    -- All alternatives are string literals - create trie node
    local strings = {}
    for _, alt in ipairs(alternatives) do
      table.insert(strings, alt.value)
    end

    replace(optimize.create_trie_node(strings))
  end)
end

-- Main entry point: apply all optimization passes
function optimize.optimize_grammar(grammar)
  grammar = optimize.trie_optimization(grammar)
  -- Future: add more optimization passes here
  return grammar
end

return optimize

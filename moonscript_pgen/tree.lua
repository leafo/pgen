-- Post-parse normalization for the pgen MoonScript grammar.
--
-- The reference parser shapes its AST with LPeg `/ fn` transformation
-- captures, which LPeg only evaluates (innermost first) once the whole
-- match succeeds. The pgen grammar instead emits those nodes tagged with
-- an "@"-prefixed marker; normalize() walks the raw tree bottom-up and
-- applies the equivalent transform, reproducing the reference's
-- evaluation order and final shapes.
--
-- Like the reference (parse/util.lua), format_assign raises
-- error({node, msg}) for invalid assignment targets; callers should pcall
-- normalize and format the error (see moonscript_pgen/init.lua).

local unpack = unpack or table.unpack

local tree = {}

-- moonscript.types.ntype, minus the "nil" case (we never pass nil)
local function ntype(node)
  if type(node) == "table" then
    return node[1]
  end
  return "value"
end
tree.ntype = ntype

local chain_assignable = {index = true, dot = true, slice = true}

local function is_assignable(node)
  if node == "..." then
    return false
  end
  local t = ntype(node)
  if t == "ref" or t == "self" or t == "value" or t == "self_class" or t == "table" then
    return true
  elseif t == "chain" then
    return chain_assignable[ntype(node[#node])]
  end
  return false
end
tree.is_assignable = is_assignable

local function flatten_or_mark(name)
  return function(tbl)
    if #tbl == 1 then
      return tbl[1]
    end
    table.insert(tbl, 1, name)
    return tbl
  end
end

local flatten_explist = flatten_or_mark("explist")

local function format_assign(lhs_exps, assign)
  if not assign then
    return flatten_explist(lhs_exps)
  end
  for _, assign_exp in ipairs(lhs_exps) do
    if not is_assignable(assign_exp) then
      error({assign_exp, "left hand expression is not assignable"})
    end
  end
  local t = ntype(assign)
  if t == "assign" then
    return {"assign", lhs_exps, unpack(assign, 2)}
  elseif t == "update" then
    return {"update", lhs_exps[1], unpack(assign, 2)}
  else
    error("unknown assign expression: " .. tostring(t))
  end
end

local function format_single_assign(lhs, assign)
  if assign then
    return format_assign({lhs}, assign)
  end
  return lhs
end

local function join_chain(callee, args)
  if #args == 0 then
    return callee
  end
  args = {"call", args}
  if ntype(callee) == "chain" then
    table.insert(callee, args)
    return callee
  end
  return {"chain", callee, args}
end

local transforms = {
  -- pos(patt): {"@pos", pos, value} -> value[-1] = pos
  ["@pos"] = function(node)
    local p, value = node[2], node[3]
    if type(value) == "table" then
      value[-1] = p
    end
    return value
  end,

  -- flatten_or_mark("exp"): single value passes through unwrapped
  ["@exp"] = function(node)
    if #node == 2 then
      return node[2]
    end
    node[1] = "exp"
    return node
  end,

  ["@assign"] = function(node)
    return format_assign(node[2], node[3])
  end,

  ["@singleassign"] = function(node)
    return format_single_assign(node[2], node[3])
  end,

  ["@chainvalue"] = function(node)
    return join_chain(node[2], node[3])
  end,

  ["@decorated"] = function(node)
    if node[3] then
      return {"decorated", node[2], node[3]}
    end
    return node[2]
  end,

  -- {:name} shorthand: {"@selfassign", name, pos}
  ["@selfassign"] = function(node)
    local name, p = node[2], node[3]
    return {
      {"key_literal", name},
      {"ref", name, [-1] = p},
    }
  end,

  -- {"@luastring", content, ["lua_eq"] = "=="} -> {"string", "[==[", content}
  ["@luastring"] = function(node)
    return {"string", "[" .. node["lua_eq"] .. "[", node[2]}
  end,
}

-- table.maxn: the class node can contain a genuine nil hole at [2]
-- (anonymous class), so #node is not a reliable iteration bound
local maxn = table.maxn or function(t)
  local n = 0
  for k in pairs(t) do
    if type(k) == "number" and k > n then
      n = k
    end
  end
  return n
end

local function normalize(node)
  if type(node) ~= "table" then
    return node
  end
  for i = 1, maxn(node) do
    node[i] = normalize(node[i])
  end
  local transform = transforms[node[1]]
  if transform then
    return transform(node)
  end
  return node
end
tree.normalize = normalize

return tree

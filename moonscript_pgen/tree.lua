-- Transform helpers for the pgen MoonScript grammar's Cfn callbacks
-- (ports of the reference parser's transformation functions from
-- moonscript/parse.lua and parse/util.lua). The callbacks themselves live
-- as code strings in grammar.lua and require this module at parser load.
--
-- Like the reference, format_assign raises error({node, msg}) for invalid
-- assignment targets; it propagates out of parse() and is formatted by
-- moonscript_pgen/init.lua.

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
tree.format_assign = format_assign
tree.format_single_assign = format_single_assign

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
tree.join_chain = join_chain

return tree

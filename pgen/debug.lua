local types = require("pgen.types")
local visitor = require("pgen.visitor")
local pgen = require("pgen")

local M = {}


function M.strip_captures(grammar)
  local empty_match = pgen.P(0)  -- Create once, reuse

  return visitor.visit_grammar(grammar, function(node, replace)
    local t = node.type

    -- Error on capture-dependent types
    if t == types.Cmb then
      error("strip_grammar: grammar contains Cmb (match backreference) which depends on captures")
    end

    if t == types.Cmt then
      error("strip_grammar: grammar contains Cmt (match-time capture) which depends on captures")
    end

    -- Unwrap capture types that have inner patterns
    if t == types.C or t == types.Ct or t == types.Cg or t == types.Cn then
      replace(node.value)
      return
    end

    -- Replace position/constant captures with empty match
    if t == types.Cp or t == types.Cc then
      replace(empty_match)
      return
    end
  end)
end


return M

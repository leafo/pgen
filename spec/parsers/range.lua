local pgen = require "pgen"
local P, R, S, V, C, Ct = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Ct

return {
  Ct(V"item"^0) * -1,

  item = C(R("09", "az", "AZ")^1) * V"space"^-1,
  space = S(" \t\n\r")
}

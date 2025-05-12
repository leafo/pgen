local pgen = require "pgen"
local P, R, S, V, C, Ct, Cp = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Ct, pgen.Cp

-- Grammar that captures both the position and content of a simple list
return {
  "list",

  list = V"ws" * Ct((V"item_with_pos")^0) * V"ws" * -1,

  item_with_pos = V"ws" * Ct(Cp() * V"item") * V"ws" * P"," * V"ws",

  item = C(V"identifier"),

  identifier = (R("az") + R("AZ"))^1,

  ws = S(" \t\n\r")^0
}

local pgen = require "pgen"
local P, R, S, V = pgen.P, pgen.R, pgen.S, pgen.V

return {
  V"ws" * V"expr" * V"ws" *  -1,

  expr = V"additive",

  -- Whitespace: optional spaces, tabs, newlines
  ws = (S" \t\n\r")^0,

  -- Additive expressions: multiplicative + additive
  additive = V"multiplicative" * V"ws" * S"+-" * V"ws" * V"additive" + V"multiplicative",

  -- Multiplicative expressions: primary * multiplicative
  multiplicative = V"primary" * V"ws" * S"*/" * V"ws" * V"multiplicative" + V"primary",

  -- Primary expressions: number or (expr)
  primary = V"number" + P"(" * V"ws" * V"expr" * V"ws" * P")",

  -- Number: integer or float, optionally negative
  number = P"-"^-1 * (V"float" + V"integer"),

  -- Integer: sequence of digits
  integer = R("0", "9")^1,

  -- Float: integer.integer
  float = V"integer" * P"." * V"integer"
}

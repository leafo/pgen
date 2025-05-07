
local pgen = require "pgen"
local P, R, S, V = pgen.P, pgen.R, pgen.S, pgen.V

return {
  "numbers", -- initial rule name

  numbers = (V"ws" * V"number")^1 * -1,

  ws = (S" \t\n\r")^0,
  number = P"-"^-1 * (V"float" + V"integer"),
  integer = R("09")^1,
  float = V"integer" * P"." * V"integer"
}

local pgen = require "pgen"
local P, V, C = pgen.P, pgen.V, pgen.C

-- Deeply recursive grammar for exercising the recursion depth limit

return {
  "expr",
  expr = P"(" * V"expr" * P")" + C(P"x"),
}

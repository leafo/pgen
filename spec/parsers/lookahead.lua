local pgen = require "pgen"
local P, L, C, V = pgen.P, pgen.L, pgen.C, pgen.V

-- Simple lookahead test grammar
return {
  V"positive" + V"negative",

  -- we put a capture in the lookahead to ensure it's removed
  positive = P"abc" * L(C(P"def")) * P"def" * -1,

  negative = P"xyz" * L(-P"def"),
}

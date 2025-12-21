local pgen = require "pgen"
local P, R, S, V, C, Ct, Cc, Cg = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Ct, pgen.Cc, pgen.Cg

-- Test grammar for Cg (capture group) functionality
return {
  "test",

  -- Test 1: Named capture becomes named field in Ct
  test = Ct(Cg(P"hello", "greeting") * P" " * C(P"world")),
}

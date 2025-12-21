local pgen = require "pgen"
local P, R, S, V, C = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C

-- Test grammar for repeat-class optimization behavior
return {
  "test",

  test = P"1:" * V"test1" +
         P"2:" * V"test2" +
         P"3:" * V"test3",

  -- Test 1: set with max=1
  test1 = (S"+-")^-1 * C(P"X"),

  -- Test 2: range with max=2
  test2 = (R"09")^-2 * C(P"X"),

  -- Test 3: single char with max=1
  test3 = (P"=")^-1 * C(P"X"),
}

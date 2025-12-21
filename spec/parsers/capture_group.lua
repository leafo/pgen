local pgen = require "pgen"
local P, V, C, Ct, Cc, Cp, Cg = pgen.P, pgen.V, pgen.C, pgen.Ct, pgen.Cc, pgen.Cp, pgen.Cg

-- Test grammar for Cg (capture group) functionality
-- Use prefix to select test: "1:...", "2:...", etc.
return {
  "test",

  test = P"1:" * V"test1" +
         P"2:" * V"test2" +
         P"3:" * V"test3" +
         P"4:" * V"test4" +
         P"5:" * V"test5" +
         P"6:" * V"test6" +
         P"7:" * V"test7" +
         P"8:" * V"test8" +
         P"9:" * V"test9",

  -- Test 1: Named capture becomes named field in Ct
  test1 = Ct(Cg(P"hello", "greeting") * P" " * C(P"world")),

  -- Test 2: Multiple named captures
  test2 = Ct(Cg(P"a", "first") * Cg(P"b", "second")),

  -- Test 3: Mixed named and positional captures
  test3 = Ct(C(P"x") * Cg(P"y", "named") * C(P"z")),

  -- Test 4: Cg with Cc (constant capture) as the pattern
  test4 = Ct(Cg(Cc("constant_value"), "const_field") * C(P"hello")),

  -- Test 5: Cg with Cc (constant capture)
  test5 = Ct(Cc("type") * Cg(P"value", "data")),

  -- Test 6: Multiple captures inside Cg - only first is used
  test6 = Ct(Cg(C(P"hi") * P" " * C(P"there"), "greeting")),

  -- Test 7: Cg outside Ct - should return only position (sentinel stripped)
  test7 = Cg(C(P"hello"), "name"),

  -- Test 8: Cg mixed with regular captures outside Ct - only C captures returned
  test8 = C(P"hello") * Cg(Cc("named"), "name") * C(P"world"),

  -- Test 9: Cg with Cc outside Ct - only position returned (no captures)
  test9 = P"x" * Cg(Cc("value"), "key") * P"y",
}

local pgen = require "pgen"
local P, V, C, Ct, Cg, Cc = pgen.P, pgen.V, pgen.C, pgen.Ct, pgen.Cg, pgen.Cc

-- Test grammar for numbered capture (patt/n) functionality
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
         P"9:" * V"test9" +
         P"10:" * V"test10" +
         P"11:" * V"test11",

  -- Test 1: Select first capture from multiple
  test1 = (C(P"a") * C(P"b") * C(P"c")) / 1,

  -- Test 2: Select second capture from multiple
  test2 = (C(P"a") * C(P"b") * C(P"c")) / 2,

  -- Test 3: Select third capture from multiple
  test3 = (C(P"a") * C(P"b") * C(P"c")) / 3,

  -- Test 4: Discard all captures with / 0
  test4 = (C(P"hello") * C(P" ") * C(P"world")) / 0,

  -- Test 5: n out of range returns nil
  test5 = (C(P"a") * C(P"b")) / 5,

  -- Test 6: Single capture with / 1
  test6 = C(P"single") / 1,

  -- Test 7: Works inside Ct
  test7 = Ct(C(P"x") * ((C(P"y") * C(P"z")) / 2)),

  -- Test 8: Nested numbered captures
  test8 = ((C(P"a") * C(P"b") * C(P"c")) / 2) / 1,

  -- Test 9: Cn skips Cg sentinels - /1 selects the only visible capture ("two")
  test9 = (Cg(Cc("one"), "a") * Cc("two") * Cg(Cc("three"), "b")) / 1,

  -- Test 10: Cn skips Cg sentinels - /2 should return nil (only 1 visible capture)
  test10 = (Cg(Cc("one"), "a") * Cc("two") * Cg(Cc("three"), "b")) / 2,

  -- Test 11: Cn with mixed C and Cg - /2 selects "z" (skipping Cg)
  test11 = (C(P"x") * Cg(C(P"y"), "name") * C(P"z")) / 2,
}

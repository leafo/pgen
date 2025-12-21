local pgen = require("pgen")
local P, V, C, S = pgen.P, pgen.V, pgen.C, pgen.S

-- Test grammar for trie optimization
-- Use prefix to select test: "1:...", "2:...", etc.
return {
  "test",

  test = P"1:" * V"test1" +
         P"2:" * V"test2" +
         P"3:" * V"test3" +
         P"4:" * V"test4" +
         P"5:" * V"test5",

  -- Test 1: Basic trie (3+ same-length literals)
  test1 = C(P"foo" + P"bar" + P"baz" + P"qux"),

  -- Test 2: Trie with fallback - tests backtracking
  -- Descending length order to trigger trie: abd(3), abc(3), ab(2)
  -- If input is "ax", trie matches "a" then fails on "x", must backtrack
  -- and match via fallback C(P"a")
  test2 = C(P"abd" + P"abc" + P"ab") + C(P"a"),

  -- Test 3: Trie with overlapping prefixes (longest match)
  -- Descending length order: fork(4), foo(3), for(3)
  test3 = C(P"fork" + P"foo" + P"for"),

  -- Test 4: Trie in a sequence
  test4 = C(P"xx" + P"xy" + P"xz") * C(S"abc"),

  -- Test 5: Trie in sequence with missing intermediate terminal
  -- Descending length order: abcd(4), ab(2), ax(2)
  -- Input "abcX" should fail (no "abc" alternative).
  test5 = C(P"abcd" + P"ab" + P"ax") * P"X",
}

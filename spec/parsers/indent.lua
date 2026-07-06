local pgen = require "pgen"
local P, R, S, V, C, Cc, Ct, Cmt, L = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Cc, pgen.Ct, pgen.Cmt, pgen.L

-- Exercises the indenter primitives (pgen.indenter): stack operations that
-- live alongside the parser and roll back transactionally on backtracking

-- default indenter: tab_width = 4, initial level 0
local ind = pgen.indenter{}

-- flag stack that starts with 1 on it ("enabled"), used with constant ops only
local flags = pgen.indenter{ initial = 1 }

-- indenter with narrow tabs
local tabs2 = pgen.indenter{ tab_width = 2 }

return {
  "test",

  test = P"10:" * V"tab4_test" +
         P"11:" * V"tab2_test" +
         P"1:" * V"block_test" +
         P"2:" * V"backtrack_test" +
         P"3:" * V"prevent_test" +
         P"4:" * V"advance_control" +
         P"5:" * V"push_test" +
         P"6:" * V"flags_test" +
         P"7:" * V"pop_test" +
         P"8:" * V"lookahead_test" +
         P"9:" * V"cmt_test",

  nl = P"\n",
  name = C(R"az"^1),

  -- 1: mini block language: lowercase names, "name:" opens an indented block
  block_test = V"Block" * -P(1),
  Block = Ct(V"Line" * (V"nl" * V"Line")^0),
  Line = ind.check * V"Stmt",
  Stmt = V"name" * (P":" * V"nl" * ind.advance * V"Block" * ind.pop)^-1,

  -- 2: backtracking across advance: first alternative pushes a level then
  -- fails, second alternative only matches if the push was rolled back
  backtrack_test = V"bt_fail" + V"bt_fallback",
  bt_fail = P"a" * V"nl" * ind.advance * P"zzz",
  bt_fallback = P"a" * V"nl" * ind.ctop("eq", 0) * S" "^0 * P"b" * Cc"clean",

  -- 3: prevent stops a nested advance from matching a deeper line
  prevent_test = ind.prevent * (V"pr_advanced" + V"pr_blocked") * ind.pop,
  pr_advanced = P"a" * V"nl" * ind.advance * S" "^0 * P"x" * ind.pop * Cc"advanced",
  pr_blocked = P"a" * V"nl" * S" "^0 * P"x" * Cc"blocked",

  -- 4: same alternatives as 3 without prevent: advance matches
  advance_control = V"pr_advanced" + V"pr_blocked",

  -- 5: unconditional push of measured width, checked on the following line
  push_test = P"{" * V"nl" * ind.push * P"x" * V"nl" * ind.check * P"y" * ind.pop * -P(1) * Cc"ok",

  -- 6: constant ops on a flag stack (do-stack style)
  allowed = flags.ctop("ne", 0) * Cc"allowed",
  flags_test = V"allowed" * flags.cpush(0) * (V"allowed" + Cc"disabled") * flags.pop * V"allowed",

  -- 7: pop semantics: the seed element can be popped, popping an empty stack
  -- fails, and pops roll back when an alternative fails
  pop_test = ind.pop * ind.pop * Cc"double" +
             ind.pop * ind.ctop("eq", 99) * Cc"top-check" +
             ind.pop * Cc"single",

  -- 8: lookahead is state-pure: the push inside L() is rolled back
  lookahead_test = L(ind.cpush(9)) * ind.ctop("eq", 0) * Cc"pure",

  -- 9: a Cmt whose callback rejects the match rolls back the inner pattern's
  -- stack operations
  cmt_test = Cmt(ind.cpush(7), [[return false]]) * Cc"never" +
             ind.ctop("eq", 0) * Cc"cmt-clean",

  -- 10/11: tab width configuration (default 4 vs 2)
  tab4_test = P"a" * V"nl" * ind.advance * ind.ctop("eq", 4) * S" \t"^0 * P"b" * ind.pop * Cc"tab4",
  tab2_test = P"a" * V"nl" * tabs2.advance * tabs2.ctop("eq", 2) * S" \t"^0 * P"b" * tabs2.pop * Cc"tab2",
}

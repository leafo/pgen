local pgen = require "pgen"
local P, V, C, Ct, Cg, Cmb = pgen.P, pgen.V, pgen.C, pgen.Ct, pgen.Cg, pgen.Cmb

return {
  "test",

  test = P"1:" * V"basic_match" +
         P"2:" * V"lua_long_string" +
         P"3:" * V"empty_match" +
         P"4:" * V"mismatch",

  -- Test 1: Basic backreference
  basic_match = Cg(P"a"^1, "as") * P":" * Cmb("as"),

  -- Test 2: Lua long string style (captures content)
  -- Wrap in Ct to collect captures into a table
  lua_long_string = Ct(P"[" * Cg(P"="^0, "eq") * P"[" *
                    C((P(1) - (P"]" * Cmb("eq") * P"]"))^0) *
                    P"]" * Cmb("eq") * P"]"),

  -- Test 3: Empty capture match
  -- Wrap in Ct to collect captures into a table
  empty_match = Ct(Cg(P"x"^0, "maybe") * Cmb("maybe") * C(P"done")),

  -- Test 4: Mismatch test (should fail)
  mismatch = Cg(P"abc", "x") * Cmb("x"),
}

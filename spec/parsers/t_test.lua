local pgen = require "pgen"
local P, V, C, T, R = pgen.P, pgen.V, pgen.C, pgen.T, pgen.R

return {
  "test",

  -- Main dispatcher
  test = P"1:" * V"basic_throw" +
         P"2:" * V"choice_with_throw" +
         P"3:" * V"conditional_throw" +
         P"4:" * V"throw_after_capture" +
         P"5:" * V"predicate_swallow",

  -- Test 1: Always throws immediately
  basic_throw = T("always_fails"),

  -- Test 2: First alternative fails normally, second throws
  choice_with_throw = P"match" + T("expected_match"),

  -- Test 3: Throws only if identifier missing after comma
  conditional_throw = V"id" * (P"," * (V"id" + T("expected_identifier")))^0,

  -- Test 4: Captures work before throw
  throw_after_capture = C(R("az")^1) * (P"!" + T("expected_exclamation")),

  -- Test 5: Negation swallows labeled failure
  predicate_swallow = -T("nope") * P"ok",

  id = C(R("az")^1),
}

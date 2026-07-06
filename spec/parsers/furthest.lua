local pgen = require "pgen"
local P, R, S, V, C, Ct, L, Cmt = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Ct, pgen.L, pgen.Cmt

-- Test grammar for furthest-failure position reporting. A miniature
-- assignment language exercising the failure paths of each terminal kind:
--   stmts  = stmt (";" stmt)* EOF
--   stmt   = name "=" value
--   value  = number | string | "func()" | checked
local ws = S" \t\n"^0

return {
  "test",

  test = P"1:" * V"stmts" +
         P"2:" * V"lookahead_case" +
         P"3:" * V"negate_case" +
         P"4:" * V"cmt_case",

  stmts = V"stmt" * (ws * P";" * V"stmt")^0 * ws * P(-1),
  stmt = ws * V"name" * ws * P"=" * ws * V"value",
  name = C(R("az")^1),
  value = V"number" + V"string" + V"call",
  number = C(R("09")^1),
  string = P"'" * C((P(1) - P"'")^0) * P"'",
  call = C(P"func" * P"()"),

  -- failures inside a successful lookahead should not dominate reporting
  lookahead_case = L(P"ab") * P"abc" * P(-1),

  -- failure via negation: input matched the forbidden pattern
  negate_case = P"x" * -P"y" * P(1) * P(-1),

  -- failure via Cmt rejection: callback fails after inner match
  cmt_case = P"a"^1 * Cmt(C(R("09")^1), [[
    local subject, pos, num = ...
    if tonumber(num) > 100 then
      return false
    end
    return true
  ]]) * P(-1),
}

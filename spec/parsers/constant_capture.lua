local pgen = require "pgen"
local P, R, S, V, C, Ct, Cp, Cc = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Ct, pgen.Cp, pgen.Cc

-- Grammar that demonstrates the use of constant captures (Cc)
-- This grammar parses key-value pairs and adds a constant type field
return {
  "pairs",

  pairs = V"ws" * Ct((V"pair")^0) * V"ws" * -1,

  pair = V"ws" * Ct(
    V"key" * V"ws" * P"=" * V"ws" * V"value" *
    Cc("pair") -- Add a constant 'type' field
  ) * V"ws" * P";" * V"ws",

  key = C(V"identifier"),

  value = V"string" + V"number" + V"boolean",

  string = P'"' * C((P(1) - P'"')^0) * P'"',

  number = C(P"-"^-1 * R("09")^1),

  boolean = C(P"true" + P"false"),

  identifier = (R("az") + R("AZ"))^1,

  ws = S(" \t\n\r")^0
}

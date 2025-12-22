local pgen = require "pgen"
local P, R, S, V, C, Cc, Ct, T = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Cc, pgen.Ct, pgen.T

-- Define a JSON grammar that captures into an AST
return {
  "json", -- initial rule name

  -- Main JSON entry rule - either an object or array
  json = V"ws" * Ct(Cc("json") * (V"value" + T("expected_value"))) * V"ws" * (-1 + T("expected_eof")),

  -- Whitespace: optional spaces, tabs, newlines
  ws = (S" \t\n\r")^0,

  -- Object: {key: value, key: value, ...}
  object = P"{" * V"ws" * Ct(Cc("object") * (V"member" * (V"ws" * P"," * V"ws" * (V"member" + T("expected_member")))^0)^-1) * V"ws" * (P"}" + T("expected_closing_brace")),

  member = Ct(Cc("member") * V"string" * V"ws" * (P":" + T("expected_colon")) * V"ws" * (V"value" + T("expected_value"))),

  -- Array: [value, value, ...]
  array = P"[" * V"ws" * Ct(Cc("array") * (V"value" * (V"ws" * P"," * V"ws" * (V"value" + T("expected_array_element")))^0)^-1) * V"ws" * (P"]" + T("expected_closing_bracket")),

  -- JSON values
  value = V"object" + V"array" + V"string" + V"number" + V"true" + V"false" + V"null",

  -- String: "..."
  string = P"\"" * Ct(Cc("string") * C(V"char"^0)) * (P"\"" + T("expected_closing_quote")),
  char = V"escape" + (P(1) - S"\"\\"),

  -- Escape sequences
  escape = P"\\" * (S"\"\\/bfnrt" + V"unicode" + T("invalid_escape_sequence")),
  unicode = P"u" * V"hex" * V"hex" * V"hex" * V"hex",
  hex = R("09", "af", "AF"),

  -- Number: int[.frac][e[+-]exp]
  number = Ct(Cc("number") * C(P"-"^-1 * (P"0" + (R"19" * R"09"^0)) * (P"." * R"09"^1)^-1 * (S"eE" * S"+-"^-1 * R"09"^1)^-1)),

  -- Constants
  ["true"] = P"true" * Ct(Cc("boolean") * Cc(true)),
  ["false"] = P"false" * Ct(Cc("boolean") * Cc(false)),
  ["null"] = P"null" * Ct(Cc("null"))
}

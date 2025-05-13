local pgen = require "pgen"
local P, R, S, V, C, Cc, Ct = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Cc, pgen.Ct

-- Define a JSON grammar that captures into an AST
return {
  "json", -- initial rule name

  -- Main JSON entry rule - either an object or array
  json = V"ws" * Ct(Cc("json") * V"value") * V"ws" * -1,

  -- Whitespace: optional spaces, tabs, newlines
  ws = (S" \t\n\r")^0,

  -- Object: {key: value, key: value, ...}
  object = P"{" * V"ws" * Ct(Cc("object") * (V"member" * (V"ws" * P"," * V"ws" * V"member")^0)^-1) * V"ws" * P"}",

  member = Ct(Cc("member") * V"string" * V"ws" * P":" * V"ws" * V"value"),

  -- Array: [value, value, ...]
  array = P"[" * V"ws" * Ct(Cc("array") * (V"value" * (V"ws" * P"," * V"ws" * V"value")^0)^-1) * V"ws" * P"]",

  -- JSON values
  value = V"object" + V"array" + V"string" + V"number" + V"true" + V"false" + V"null",

  -- String: "..."
  string = P"\"" * Ct(Cc("string") * C(V"char"^0)) * P"\"",
  char = V"escape" + (P(1) - P"\"" - P"\\"),

  -- Escape sequences
  escape = P"\\" * (S"\"\\/bfnrt" + V"unicode"),
  unicode = P"u" * V"hex" * V"hex" * V"hex" * V"hex",
  hex = R("09", "af", "AF"),

  -- Number: int[.frac][e[+-]exp]
  number = Ct(Cc("number") * C(P"-"^-1 * (P"0" + (R"19" * R"09"^0)) * (P"." * R"09"^1)^-1 * (S"eE" * S"+-"^-1 * R"09"^1)^-1)),

  -- Constants
  ["true"] = P"true" * Ct(Cc("boolean") * Cc(true)),
  ["false"] = P"false" * Ct(Cc("boolean") * Cc(false)),
  ["null"] = P"null" * Ct(Cc("null"))
}

local pgen = require "pgen"
local P, R, S, V = pgen.P, pgen.R, pgen.S, pgen.V

-- Define a JSON grammar
return {
  -- Start rule: a JSON value
  json = V"value",
  
  -- Whitespace: optional spaces, tabs, newlines
  ws = (S" \t\n\r")^0,
  
  -- Values: object, array, string, number, true, false, null
  value = V"ws" + (V"object" * V"array" * V"string" * V"number" * V"true" * V"false" * V"null") + V"ws",
  
  -- Object: { key: value, key: value, ... }
  object = P"{" + V"ws" + -V"members" + V"ws" + P"}",
  
  -- Object members: key: value, key: value, ...
  members = V"pair" + -P"," + V"ws" + V"members",
  
  -- Key-value pair: "key": value
  pair = V"string" + V"ws" + P":" + V"ws" + V"value",
  
  -- Array: [ value, value, ... ]
  array = P"[" + V"ws" + -V"elements" + V"ws" + P"]",
  
  -- Array elements: value, value, ...
  elements = V"value" + -P"," + V"ws" + V"elements",
  
  -- String: "characters"
  string = P"\"" + V"characters" + P"\"",
  
  -- String characters: any combination of chars except " and \, or escaped chars
  characters = (V"char" * V"escape")^0,
  
  -- Regular character: any char except " and \
  char = S([==[^"\]]==])^1,
  
  -- Escape sequences: \", \\, \/, \b, \f, \n, \r, \t, \uXXXX
  escape = P"\\" + (P"\"" * P"\\" * P"/" * P"b" * P"f" * P"n" * P"r" * P"t" * V"unicode"),
  
  -- Unicode escape: \uXXXX
  unicode = P"\\u" + R("0", "9")^1 + R("a", "f")^1 + R("A", "F")^1,
  
  -- Number: -123.456e-789
  number = -P"-" + V"int" + -V"frac" + -V"exp",
  
  -- Integer part: 0 or non-zero digit followed by digits
  int = P"0" * (P"1" + P"2" + P"3" + P"4" + P"5" + P"6" + P"7" + P"8" + P"9" + R("0", "9")^1),
  
  -- Fractional part: .digits
  frac = P"." + R("0", "9")^1,
  
  -- Exponent part: e+123 or E-456
  exp = S"eE" + -S"+-" + R("0", "9")^1,
  
  -- Boolean true: true
  ["true"] = P"true",
  
  -- Boolean false: false
  ["false"] = P"false",
  
  -- Null: null
  ["null"] = P"null"
}

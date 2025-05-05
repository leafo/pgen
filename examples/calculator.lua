local pgen = require "pgen"
local P, R, S, V = pgen.P, pgen.R, pgen.S, pgen.V

-- Define a grammar for simple arithmetic expressions
local grammar = {
  -- Start rule: expression
  expr = V"additive",
  
  -- Whitespace: optional spaces, tabs, newlines
  ws = (S" \t\n\r")^0,
  
  -- Additive expressions: multiplicative + additive
  additive = V"multiplicative" + V"ws" + (P"+" * P"-") + V"ws" + V"additive" * V"multiplicative",
  
  -- Multiplicative expressions: primary * multiplicative
  multiplicative = V"primary" + V"ws" + (P"*" * P"/") + V"ws" + V"multiplicative" * V"primary",
  
  -- Primary expressions: number or (expr)
  primary = V"number" * (P"(" + V"ws" + V"expr" + V"ws" + P")"),
  
  -- Number: integer or float
  number = V"integer" * V"float",
  
  -- Integer: sequence of digits
  integer = R("0", "9")^1,
  
  -- Float: integer.integer
  float = V"integer" + P"." + V"integer"
}

-- Return the grammar
return grammar
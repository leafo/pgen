local pgen = require "pgen"
local P, R, S, V, C, Cc, Ct = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Cc, pgen.Ct

-- MoonScript prototype parser focusing on indentation-based parsing
-- Indentation state is a pgen indenter stack that lives alongside the parser
-- and rolls back transactionally when the parser backtracks.
-- Each space = 1, each tab = 2 (simplified; real MoonScript uses tab = 4)

local ind = pgen.indenter{ tab_width = 2 }

return {
  "File",

  -- Basic whitespace (no newlines)
  ws = S" \t"^0,

  -- Newline
  nl = P"\n",

  -- Blank line (optional indentation + newline), ignored
  BlankLine = S" \t"^0 * V"nl",

  -- Number literal
  Number = Ct(Cc"number" * C(R"09"^1 * (P"." * R"09"^1)^-1)),

  -- Identifier
  ident = R("az", "AZ", "__") * R("az", "AZ", "09", "__")^0,

  -- Name (identifier)
  Name = Ct(Cc"name" * C(V"ident")),

  -- String literals
  String = Ct(Cc"string" * (
    P'"' * C((P'\\"' + (P(1) - P'"'))^0) * P'"' +
    P"'" * C((P"\\'" + (P(1) - P"'"))^0) * P"'"
  )),

  -- Value: number, string, or name
  Value = V"Number" + V"String" + V"Name",

  -- Assignment statement
  Assign = Ct(Cc"assign" * V"Name" * V"ws" * P"=" * V"ws" * V"Value"),

  -- Simple statement (if, assignment, or bare value expression)
  Statement = V"IfStatement" + V"Assign" + V"Value",

  -- If statement with indented block
  IfStatement = Ct(Cc"if" * P"if" * S" \t"^1 * V"Value" * V"InBlock"),

  -- Indented block: newline, optional blank lines, deeper indent, statements
  -- at the new level, then pop back out
  InBlock = V"nl" * V"BlankLine"^0 * ind.advance * V"Block" * ind.pop,

  -- Block: sequence of lines at current indent level, skipping blank lines
  Block = Ct(Cc"block" * (V"BlankLine" + V"Line")^1 * (V"BlankLine" + (V"nl" * V"Line"))^0),

  -- Line: check indent, then statement
  -- Only match if indent equals current level
  Line = ind.check * V"Statement",

  -- File: top-level block (indent level 0)
  File = V"Block" * V"ws" * -P(1),
}

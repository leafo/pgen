local pgen = require "pgen"
local P, R, S, V, C, Cc, Ct = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Cc, pgen.Ct
local layout = pgen.layout({
  tab_width = 2,
  allow_tabs = true,
  allow_mixed = true
})
local NL, Indent, Dedent, SameIndent, BlankLine = layout.NL, layout.Indent, layout.Dedent, layout.SameIndent, layout.BlankLine

-- MoonScript prototype parser focusing on indentation-based parsing

return {
  "File",

  -- Basic whitespace (no newlines)
  ws = S" \t"^0,

  -- Newline (layout-aware), skipping blank lines
  nl = NL * V"BlankLine"^0,

  -- Blank line (optional indentation + newline), ignored
  BlankLine = BlankLine,

  -- Indent controls
  SameIndent = SameIndent,
  Indent = Indent,
  Dedent = Dedent,

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

  -- Indented block: newline, indent, statements, pop indent
  InBlock = V"nl" * V"Indent" * V"Block" * V"Dedent",

  -- Block: sequence of lines at current indent level
  Block = Ct(Cc"block" * V"Line" * (V"nl" * V"Line")^0),

  -- Line: check indent, then statement
  -- Only match if indent equals current level
  Line = V"SameIndent" * V"Statement",

  -- File: top-level block (indent level 0)
  File = V"BlankLine"^0 * V"Block" * V"nl"^0 * V"ws" * -P(1),
}

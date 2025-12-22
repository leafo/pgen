local pgen = require "pgen"
local P, R, S, V, C, Cc, Ct, Cp, Cg, Cmt, L = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Cc, pgen.Ct, pgen.Cp, pgen.Cg, pgen.Cmt, pgen.L

-- MoonScript prototype parser focusing on indentation-based parsing
-- State is managed via _G._ms_indent (must be initialized before parsing)

return {
  "File",

  -- Basic whitespace (no newlines)
  ws = S" \t"^0,

  -- Newline
  nl = P"\n",

  -- Blank line (optional indentation + newline), ignored
  BlankLine = S" \t"^0 * V"nl",

  -- Calculate indent level from whitespace string
  -- Each space = 1, each tab = 2 (simplified)
  CalcIndent = Cmt(C(S" \t"^0), [[
    local subject, pos, spaces = ...
    local level = 0
    for i = 1, #spaces do
      local c = spaces:sub(i, i)
      if c == " " then
        level = level + 1
      elseif c == "\t" then
        level = level + 2
      end
    end
    return true, level
  ]]),

  -- Check that current indent matches top of stack
  CheckIndent = Cmt(V"CalcIndent", [[
    local subject, pos, level = ...
    local stack = _G._ms_indent
    return stack[#stack] == level
  ]]) / 0,

  -- Advance: push new indent level if greater than current
  Advance = L(Cmt(V"CalcIndent", [[
    local subject, pos, level = ...
    local stack = _G._ms_indent
    local top = stack[#stack]
    if level > top then
      stack[#stack + 1] = level
      return true
    end
    return false
  ]])),

  -- Pop indent stack
  PopIndent = Cmt(P"", [[
    local subject, pos = ...
    local stack = _G._ms_indent
    if #stack > 1 then
      stack[#stack] = nil
      return true
    end
    return true  -- Don't fail if already at base
  ]]) / 0,

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

  -- Indented block: newline, optional blank lines, advance indent, statements, pop indent
  InBlock = V"nl" * V"BlankLine"^0 * V"Advance" * V"Block" * V"PopIndent",

  -- Block: sequence of lines at current indent level, skipping blank lines
  Block = Ct(Cc"block" * (V"BlankLine" + V"Line")^1 * (V"BlankLine" + (V"nl" * V"Line"))^0),

  -- Line: check indent, then statement
  -- Only match if indent equals current level
  Line = V"CheckIndent" * V"Statement",

  -- File: top-level block (indent level 0)
  File = V"Block" * V"ws" * -P(1),
}

-- MoonScript grammar for pgen, ported from moonscript/moonscript/parse.lua
--
-- The reference parser builds its AST with LPeg `/ fn` transformation
-- captures that run after the match completes. pgen has no function
-- captures, so nodes that need a transform are emitted as tables whose
-- first element is an "@"-prefixed tag (e.g. "@exp", "@chainvalue"); the
-- post-parse pass in moonscript_pgen/tree.lua resolves them bottom-up,
-- which reproduces LPeg's deferred, innermost-first evaluation order.
-- Nodes tagged with a plain name ("if", "chain", ...) are already in their
-- final shape.
--
-- The reference's two Cmt-managed stacks (indentation and the `do`
-- disambiguation stack) become pgen indenters, so none of the original's
-- ensure()/Cut cleanup patterns are needed: stack operations are undone
-- automatically when the parser backtracks.

local pgen = require "pgen"
local P, R, S, V, C, Ct, Cc, Cp, Cg, Cmb, Cmt, L =
  pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Ct, pgen.Cc, pgen.Cp,
  pgen.Cg, pgen.Cmb, pgen.Cmt, pgen.L

-- indentation stack: space = 1, tab = 4, starts with 0 on it (matches the
-- reference's count_width and `Stack(0)`)
local ind = pgen.indenter{tab_width = 4, initial = 0}

-- `do` permission stack: the `do` expression is disabled while parsing the
-- header expression of while/with/switch/for. 0 = disabled; starts with 1
-- (allowed), matching the reference where only an explicit `false` on top
-- of the stack blocks `do`.
local dos = pgen.indenter{initial = 1}
local DisableDo = dos.cpush(0)
local PopDo = dos.pop
local CheckDo = dos.ctop("ne", 0)

local AlphaNum = R("az", "AZ", "09", "__")

-- Every name registered through key()/op() in the reference grammar.
-- Sorted longest-first: pgen's trie optimization requires longer strings
-- before their prefixes, and PEG ordered choice needs "elseif" tried
-- before "else" anyway.
local keyword_words = {
  "continue", "extends",
  "elseif", "export", "import", "return", "switch", "unless",
  "break", "class", "local", "using", "while",
  "else", "from", "then", "when", "with",
  "and", "for", "not",
  "do", "if", "in", "or",
}

local keyword_patt
for _, word in ipairs(keyword_words) do
  keyword_patt = keyword_patt and (keyword_patt + P(word)) or P(word)
end
local Keyword = keyword_patt * -AlphaNum

-- Ct(Cc(name) * patt): same shape as the reference's `patt / mark(name)`
local function mark(name, patt)
  return Ct(Cc(name) * patt)
end

-- position-annotated node: resolved by tree.lua to node[-1] = pos
local function pos(patt)
  return Ct(Cc"@pos" * Cp() * patt)
end

local function sym(chars)
  return V"Space" * P(chars)
end

-- operator token: captures its text; word operators ("or", "and") must not
-- be followed by a letter, digit, or underscore
local function op(chars)
  local patt = V"Space" * C(P(chars))
  if chars:match("^%w*$") then
    patt = patt * -AlphaNum
  end
  return patt
end

local function key(chars)
  return V"Space" * P(chars) * -AlphaNum
end

return {
  "Root",

  -- the reference wraps the grammar as White * File * White * -1
  Root = V"White" * V"File" * V"White" * P(-1),

  -- lexical layer (moonscript/parse/literals.lua)
  White = S" \t\r\n"^0,
  Break = P"\r"^-1 * P"\n",
  Stop = V"Break" + P(-1),
  Comment = P"--" * (P(1) - S"\r\n")^0 * L(V"Stop"),
  Space = S" \t"^0 * V"Comment"^-1,
  SomeSpace = S" \t"^1 * V"Comment"^-1,
  SpaceBreak = V"Space" * V"Break",
  EmptyLine = V"SpaceBreak",
  Shebang = P"#!" * (P(1) - V"Stop")^0,

  NameRaw = C(R("az", "AZ", "__") * AlphaNum^0),
  Name = V"Space" * -Keyword * V"NameRaw",

  Num = V"Space" * Ct(Cc"number" * C(
    P"0x" * R("09", "af", "AF")^1 * (S"uU"^-1 * S"lL"^2)^-1 +
    R"09"^1 * (S"uU"^-1 * S"lL"^2) +
    (R"09"^1 * (P"." * R"09"^1)^-1 + P"." * R"09"^1) *
      (S"eE" * P"-"^-1 * R"09"^1)^-1
  )),

  SelfName = V"Space" * P"@" * (
    P"@" * (mark("self_class", V"NameRaw") + Cc"self.__class") +
    mark("self", V"NameRaw") + Cc"self"),
  KeyName = V"SelfName" + V"Space" * mark("key_literal", V"NameRaw"),
  VarArg = V"Space" * C(P"..."),

  -- blocks
  File = V"Shebang"^-1 * (V"Block" + Ct(P"")),
  Block = Ct(V"Line" * (V"Break"^1 * V"Line")^0),
  CheckIndent = ind.check,
  Line = V"CheckIndent" * V"Statement" + V"Space" * L(V"Stop"),

  Statement = Ct(Cc"@decorated" *
    pos(
      V"Import" + V"While" + V"With" + V"For" + V"ForEach" + V"Switch" +
      V"Return" + V"Local" + V"Export" + V"BreakLoop" +
      Ct(Cc"@assign" * Ct(V"ExpList") * (V"Update" + V"Assign")^-1)
    ) * V"Space" *
    ((mark("if", key"if" * V"Exp" * (key"else" * V"Exp")^-1 * V"Space") +
      mark("unless", key"unless" * V"Exp") +
      mark("comprehension", V"CompInner")) * V"Space")^-1),

  Body = V"Space" * V"Break" * V"EmptyLine"^0 * V"InBlock" + Ct(V"Statement"),

  Advance = ind.advance,
  PushIndent = ind.push,
  PreventIndent = ind.prevent,
  PopIndent = ind.pop,
  InBlock = V"Advance" * V"Block" * V"PopIndent",

  Local = key"local" * (
    mark("declare_glob", op"*" + op"^") +
    mark("declare_with_shadows", Ct(V"NameList"))),

  Import = mark("import",
    key"import" * Ct(V"ImportNameList") * V"SpaceBreak"^0 * key"from" * V"Exp"),
  ImportName = sym"\\" * Ct(Cc"colon" * V"Name") + V"Name",
  ImportNameList = V"SpaceBreak"^0 * V"ImportName" *
    ((V"SpaceBreak"^1 + sym"," * V"SpaceBreak"^0) * V"ImportName")^0,

  BreakLoop = Ct(key"break" * Cc"break") + Ct(key"continue" * Cc"continue"),

  Return = mark("return",
    key"return" * (mark("explist", V"ExpListLow") + C(P""))),

  WithExp = Ct(Cc"@assign" * Ct(V"ExpList") * V"Assign"^-1),
  With = mark("with",
    key"with" * DisableDo * V"WithExp" * PopDo * key"do"^-1 * V"Body"),

  Switch = mark("switch",
    key"switch" * DisableDo * V"Exp" * PopDo * key"do"^-1 *
    V"Space" * V"Break" * V"SwitchBlock"),
  SwitchBlock = V"EmptyLine"^0 * V"Advance" *
    Ct(V"SwitchCase" * (V"Break"^1 * V"SwitchCase")^0 *
      (V"Break"^1 * V"SwitchElse")^-1) * V"PopIndent",
  SwitchCase = mark("case",
    key"when" * Ct(V"ExpList") * key"then"^-1 * V"Body"),
  SwitchElse = mark("else", key"else" * V"Body"),

  IfCond = Ct(Cc"@singleassign" * V"Exp" * V"Assign"^-1),
  IfElse = mark("else",
    (V"Break" * V"EmptyLine"^0 * V"CheckIndent")^-1 * key"else" * V"Body"),
  IfElseIf = mark("elseif",
    (V"Break" * V"EmptyLine"^0 * V"CheckIndent")^-1 * key"elseif" *
    pos(V"IfCond") * key"then"^-1 * V"Body"),
  If = mark("if",
    key"if" * V"IfCond" * key"then"^-1 * V"Body" *
    V"IfElseIf"^0 * V"IfElse"^-1),
  Unless = mark("unless",
    key"unless" * V"IfCond" * key"then"^-1 * V"Body" *
    V"IfElseIf"^0 * V"IfElse"^-1),

  While = mark("while",
    key"while" * DisableDo * V"Exp" * PopDo * key"do"^-1 * V"Body"),

  For = mark("for",
    key"for" * DisableDo * V"Name" * sym"=" *
    Ct(V"Exp" * sym"," * V"Exp" * (sym"," * V"Exp")^-1) * PopDo *
    key"do"^-1 * V"Body"),
  ForEach = mark("foreach",
    key"for" * Ct(V"AssignableNameList") * key"in" * DisableDo *
    Ct(sym"*" * mark("unpack", V"Exp") + V"ExpList") * PopDo *
    key"do"^-1 * V"Body"),

  Do = mark("do", key"do" * V"Body"),

  Comprehension = mark("comprehension",
    sym"[" * V"Exp" * V"CompInner" * sym"]"),
  TblComprehension = mark("tblcomprehension",
    sym"{" * Ct(V"Exp" * (sym"," * V"Exp")^-1) * V"CompInner" * sym"}"),

  CompInner = Ct((V"CompForEach" + V"CompFor") * V"CompClause"^0),
  CompForEach = mark("foreach",
    key"for" * Ct(V"AssignableNameList") * key"in" *
    (sym"*" * mark("unpack", V"Exp") + V"Exp")),
  -- the reference builds this as key("for" * Name * ...): a bare P"for"
  -- with the -AlphaNum boundary at the very end of the pattern
  CompFor = V"Space" * Ct(Cc"for" * P"for" * V"Name" * sym"=" *
    Ct(V"Exp" * sym"," * V"Exp" * (sym"," * V"Exp")^-1)) * -AlphaNum,
  CompClause = V"CompFor" + V"CompForEach" + mark("when", key"when" * V"Exp"),

  Assign = mark("assign", sym"=" * (
    Ct(V"With" + V"If" + V"Switch") +
    Ct(V"TableBlock" + V"ExpListLow"))),

  Update = mark("update",
    V"Space" * C(
      P"..=" + P"+=" + P"-=" + P"*=" + P"/=" + P"%=" +
      P"or=" + P"and=" + P"&=" + P"|=" + P">>=" + P"<<=") * V"Exp"),

  CharOperators = V"Space" * C(S"+-*/%^><|&"),
  WordOperators =
    op"or" + op"and" + op"<=" + op">=" + op"~=" + op"!=" + op"==" +
    op".." + op"<<" + op">>" + op"//",
  BinaryOperator = (V"WordOperators" + V"CharOperators") * V"SpaceBreak"^0,

  -- match-time assignability check: a chain is assignable when its final
  -- item is a dot/index/slice (reference: check_assignable in parse/util)
  Assignable = Cmt(V"Chain", [[
    local subject, pos, node = ...
    local last = node[#node]
    local t = type(last) == "table" and last[1]
    if t == "dot" or t == "index" or t == "slice" then
      return pos, node
    end
    return false
  ]]) + V"Name" + V"SelfName",

  Exp = Ct(Cc"@exp" * V"Value" * (V"BinaryOperator" * V"Value")^0),

  SimpleValue =
    V"If" + V"Unless" + V"Switch" + V"With" + V"ClassDecl" +
    V"ForEach" + V"For" + V"While" +
    CheckDo * V"Do" +
    mark("minus", sym"-" * -V"SomeSpace" * V"Exp") +
    mark("length", sym"#" * V"Exp") +
    mark("bitnot", sym"~" * V"Exp") +
    mark("not", key"not" * V"Exp") +
    V"TblComprehension" + V"TableLit" + V"Comprehension" + V"FunLit" + V"Num",

  ChainValue = Ct(Cc"@chainvalue" *
    (V"Chain" + V"Callable") * Ct(V"InvokeArgs"^-1)),

  Value = pos(
    V"SimpleValue" +
    mark("table", Ct(V"KeyValueList")) +
    V"ChainValue" + V"String"),
  SliceValue = V"Exp",

  -- NOTE: this grammar deliberately contains no T() labels. A label turns
  -- a failure that would normally backtrack into an error for the whole
  -- parse, and in this grammar there is no place where that is safe: Value
  -- tries several alternatives before String, and those attempts can end
  -- up parsing arbitrary text as code. For example, at "[[" the
  -- Comprehension alternative consumes both brackets and then tries to
  -- parse the long string's *content* as an expression before String ever
  -- runs. A label reached during one of these attempts fails input that a
  -- later alternative would have parsed fine: `y = [[if x then import a]]
  -- .. z` is valid MoonScript, but an expected_from label inside Import
  -- rejected it (found by comparing against the reference parser over
  -- truncated real files). Error positions come from the engine's
  -- furthest-failure tracking instead.
  String = V"Space" * V"DoubleString" + V"Space" * V"SingleString" + V"LuaString",
  SingleString = mark("string",
    C(P"'") * C((P"\\'" + P"\\\\" + (P(1) - P"'"))^0) * P"'"),
  DoubleString = mark("string",
    C(P'"') *
    (C((V"DoubleStringInner" - V"DoubleStringInterp")^1) + V"DoubleStringInterp")^0 *
    P'"'),
  DoubleStringInner = P'\\"' + P"\\\\" + (P(1) - P'"'),
  DoubleStringInterp = mark("interpolate", P"#{" * V"Exp" * sym"}"),

  -- Lua long strings: the "=" run is captured as a named group and the
  -- closing delimiter matched with a backreference ("]" .. eq .. "]").
  -- tree.lua rebuilds the reference's open-delimiter string from lua_eq.
  LuaString = Ct(Cc"@luastring" *
    V"Space" * P"[" * Cg(P"="^0, "lua_eq") * P"[" *
    V"Break"^-1 * C((P(1) - V"LuaStringClose")^0) * V"LuaStringClose"),
  LuaStringClose = P"]" * Cmb"lua_eq" * P"]",

  Callable = pos(mark("ref", V"Name")) + V"SelfName" + V"VarArg" +
    mark("parens", V"Parens"),
  Parens = sym"(" * V"SpaceBreak"^0 * V"Exp" * V"SpaceBreak"^0 * sym")",

  FnArgs = P"(" * V"SpaceBreak"^0 * Ct(V"FnArgsExpList"^-1) *
      V"SpaceBreak"^0 * sym")" +
    sym"!" * -P"=" * Ct(P""),
  FnArgsExpList = V"Exp" * ((V"Break" + sym",") * V"White" * V"Exp")^0,

  Chain = Ct(Cc"chain" *
      (V"Callable" + V"String" + -S".\\") * V"ChainItems") +
    Ct(Cc"chain" *
      V"Space" * (V"DotChainItem" * V"ChainItems"^-1 + V"ColonChain")),

  ChainItems = V"ChainItem"^1 * V"ColonChain"^-1 + V"ColonChain",
  ChainItem = V"Invoke" + V"DotChainItem" + V"Slice" +
    mark("index", P"[" * V"Exp") * sym"]",
  DotChainItem = mark("dot", P"." * V"NameRaw"),
  ColonChainItem = mark("colon", P"\\" * V"NameRaw"),
  ColonChain = V"ColonChainItem" * (V"Invoke" * V"ChainItems"^-1)^-1,

  Slice = mark("slice",
    P"[" * (V"SliceValue" + Cc(1)) * sym"," * (V"SliceValue" + Cc"") *
    (sym"," * V"SliceValue")^-1 * sym"]"),

  Invoke = mark("call", V"FnArgs") +
    Ct(Cc"call" * Ct(V"SingleString")) +
    Ct(Cc"call" * Ct(V"DoubleString")) +
    L(P"[") * Ct(Cc"call" * Ct(V"LuaString")),

  TableValue = V"KeyValue" + Ct(V"Exp"),
  TableLit = mark("table",
    sym"{" * Ct(
      V"TableValueList"^-1 * sym","^-1 *
      (V"SpaceBreak" * V"TableLitLine" *
        (sym","^-1 * V"SpaceBreak" * V"TableLitLine")^0 * sym","^-1)^-1
    ) * V"White" * sym"}"),
  TableValueList = V"TableValue" * (sym"," * V"TableValue")^0,
  -- reference: PushIndent * ((TableValueList * PopIndent) + (PopIndent * Cut)) + Space
  -- the pop-and-fail alternative is not needed since the stack is undone
  -- automatically on backtracking
  TableLitLine = V"PushIndent" * V"TableValueList" * V"PopIndent" + V"Space",

  TableBlockInner = Ct(V"KeyValueLine" * (V"SpaceBreak"^1 * V"KeyValueLine")^0),
  TableBlock = mark("table",
    V"SpaceBreak"^1 * V"Advance" * V"TableBlockInner" * V"PopIndent"),

  ClassDecl = mark("class",
    key"class" * -P":" *
    (V"Assignable" + Cc(nil)) *
    (key"extends" * V"PreventIndent" * V"Exp" * V"PopIndent" + C(P""))^-1 *
    (V"ClassBlock" + Ct(P""))),

  ClassBlock = V"SpaceBreak"^1 * V"Advance" *
    Ct(V"ClassLine" * (V"SpaceBreak"^1 * V"ClassLine")^0) * V"PopIndent",
  ClassLine = V"CheckIndent" * (
    (mark("props", V"KeyValueList") +
     mark("stm", V"Statement") +
     mark("stm", V"Exp")) * sym","^-1),

  Export = mark("export", key"export" * (
    Cc"class" * V"ClassDecl" +
    op"*" + op"^" +
    Ct(V"NameList") * (sym"=" * Ct(V"ExpListLow"))^-1)),

  KeyValue =
    Ct(Cc"@selfassign" * sym":" * -V"SomeSpace" * V"Name" * Cp()) +
    Ct((V"KeyName" + sym"[" * V"Exp" * sym"]" +
        V"Space" * V"DoubleString" + V"Space" * V"SingleString") *
      P":" *
      (V"Exp" + V"TableBlock" + V"SpaceBreak"^1 * V"Exp")),
  KeyValueList = V"KeyValue" * (sym"," * V"KeyValue")^0,
  KeyValueLine = V"CheckIndent" * V"KeyValueList" * sym","^-1,

  FnArgsDef = sym"(" * V"White" * Ct(V"FnArgDefList"^-1) *
      (key"using" * Ct(V"NameList" + V"Space" * P"nil") + Ct(P"")) *
      V"White" * sym")" +
    Ct(P"") * Ct(P""),
  FnArgDefList = V"FnArgDef" *
      ((sym"," + V"Break") * V"White" * V"FnArgDef")^0 *
      ((sym"," + V"Break") * V"White" * Ct(V"VarArg"))^0 +
    Ct(V"VarArg"),
  FnArgDef = Ct((V"Name" + V"SelfName") * (sym"=" * V"Exp")^-1),

  FunLit = mark("fndef",
    V"FnArgsDef" *
    (sym"->" * Cc"slim" + sym"=>" * Cc"fat") *
    (V"Body" + Ct(P""))),

  NameList = V"Name" * (sym"," * V"Name")^0,
  NameOrDestructure = V"Name" + V"TableLit",
  AssignableNameList = V"NameOrDestructure" * (sym"," * V"NameOrDestructure")^0,

  ExpList = V"Exp" * (sym"," * V"Exp")^0,
  ExpListLow = V"Exp" * ((sym"," + sym";") * V"Exp")^0,

  InvokeArgs = -P"-" * (
    V"ExpList" *
      (sym"," * (V"TableBlock" + V"SpaceBreak" * V"Advance" * V"ArgBlock" * V"TableBlock"^-1) +
       V"TableBlock")^-1 +
    V"TableBlock"),
  ArgBlock = V"ArgLine" * (sym"," * V"SpaceBreak" * V"ArgLine")^0 * V"PopIndent",
  ArgLine = V"CheckIndent" * V"ExpList",
}

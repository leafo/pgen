local pgen = require "pgen"
local P, R, S, V, C, Cc, Ct, Cg, Cmb, L = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Cc, pgen.Ct, pgen.Cg, pgen.Cmb, pgen.L

local function idchar()
  return R("az", "AZ", "09", "__")
end

-- match a word only when it isn't a prefix of a longer identifier
local function kw(word)
  return P(word) * -idchar()
end

-- Teal grammar that captures into an AST, based on
-- https://github.com/teal-language/tl/blob/main/docs/src/grammar.md
-- It extends the Lua grammar in spec/parsers/lua_parser.lua with Teal's type
-- syntax (typed declarations, records, interfaces, enums, type aliases,
-- as/is expressions, macroexp).
--
-- "as", "is" and "global" are reserved words. "record", "interface", "enum",
-- "type", "where", "macroexp", "userdata" and "metamethod" are contextual
-- and remain usable as names, matching Teal's Lua compatibility.
--
-- Some constructs used by the tl compiler sources are missing from the
-- grammar doc and are supported here: shebang lines, interface record
-- entries, inline array records (a table type as a record entry),
-- constrained type arguments (<T is Type>), standalone `? type` parameter
-- types and `type...` variadic parameter types.
--
-- Like the Lua grammar, binary operators (and as/is) are captured as a flat
-- list without precedence.
return {
  "chunk",

  chunk = (P"#" * (P(1) - P"\n")^0)^-1 * V"block" * -1,

  block = Ct(Cc("block") * V"ws" * V"stat"^0 * V"retstat"^-1 * V"ws"),

  -- Statements. The local_*/global_* declaration forms must come before the
  -- plain local/global forms since their introducing words are contextual
  -- and would otherwise parse as variable names.
  stat = V"ws" * (
    P";" * Ct(Cc("empty")) +
    Ct(Cc("assign") * V"varlist" * V"ws" * P"=" * V"ws" * V"explist") +
    V"functioncall" +
    Ct(Cc("label") * P"::" * V"ws" * V"Name" * V"ws" * P"::") +
    Ct(Cc("break") * kw"break") +
    Ct(Cc("goto") * kw"goto" * V"ws" * V"Name") +
    Ct(Cc("do") * kw"do" * V"block" * kw"end") +
    Ct(Cc("while") * kw"while" * V"ws" * V"exp" * V"ws" * kw"do" * V"block" * kw"end") +
    Ct(Cc("repeat") * kw"repeat" * V"block" * kw"until" * V"ws" * V"exp") +
    Ct(Cc("if") * kw"if" * V"ws" * V"exp" * V"ws" * kw"then" * V"block" *
      (kw"elseif" * V"ws" * V"exp" * V"ws" * kw"then" * V"block")^0 *
      (kw"else" * V"block")^-1 * kw"end") +
    Ct(Cc("for_num") * kw"for" * V"ws" * V"Name" * V"ws" * P"=" * V"ws" * V"exp" * V"ws" * P"," * V"ws" * V"exp" *
      (V"ws" * P"," * V"ws" * V"exp")^-1 * V"ws" * kw"do" * V"block" * kw"end") +
    Ct(Cc("for_in") * kw"for" * V"ws" * V"namelist" * V"ws" * kw"in" * V"ws" * V"explist" * V"ws" * kw"do" * V"block" * kw"end") +
    Ct(Cc("function") * kw"function" * V"ws" * V"funcname" * V"funcbody") +
    Ct(Cc("local_function") * kw"local" * V"ws" * kw"function" * V"ws" * V"Name" * V"funcbody") +
    Ct(Cc("local_record") * kw"local" * V"ws" * kw"record" * V"ws" * V"Name" * V"recordbody") +
    Ct(Cc("local_interface") * kw"local" * V"ws" * kw"interface" * V"ws" * V"Name" * V"recordbody") +
    Ct(Cc("local_enum") * kw"local" * V"ws" * kw"enum" * V"ws" * V"Name" * V"enumbody") +
    Ct(Cc("local_type") * kw"local" * V"ws" * kw"type" * V"ws" * V"Name" * (V"ws" * V"typeargs")^-1 * V"ws" * P"=" * V"ws" * V"newtype") +
    Ct(Cc("local_macroexp") * kw"local" * V"ws" * kw"macroexp" * V"ws" * V"Name" * V"macroexpbody") +
    Ct(Cc("local") * kw"local" * V"ws" * V"attnamelist" *
      (V"ws" * P":" * V"ws" * V"typelist")^-1 *
      (V"ws" * P"=" * V"ws" * V"explist")^-1) +
    Ct(Cc("global_function") * kw"global" * V"ws" * kw"function" * V"ws" * V"Name" * V"funcbody") +
    Ct(Cc("global_record") * kw"global" * V"ws" * kw"record" * V"ws" * V"Name" * V"recordbody") +
    Ct(Cc("global_interface") * kw"global" * V"ws" * kw"interface" * V"ws" * V"Name" * V"recordbody") +
    Ct(Cc("global_enum") * kw"global" * V"ws" * kw"enum" * V"ws" * V"Name" * V"enumbody") +
    Ct(Cc("global_type") * kw"global" * V"ws" * kw"type" * V"ws" * V"Name" * (V"ws" * V"typeargs")^-1 * (V"ws" * P"=" * V"ws" * V"newtype")^-1) +
    -- a global declaration requires a type annotation, a value, or both
    Ct(Cc("global") * kw"global" * V"ws" * V"attnamelist" * (
      V"ws" * P":" * V"ws" * V"typelist" * (V"ws" * P"=" * V"ws" * V"explist")^-1 +
      V"ws" * P"=" * V"ws" * V"explist"))
  ) * V"ws",

  retstat = Ct(Cc("return") * kw"return" * (V"ws" * V"explist")^-1 * (V"ws" * P";")^-1),

  -- the ":" marker distinguishes a method name from the dotted path
  funcname = Ct(Cc("funcname") * V"Name" * (V"ws" * P"." * V"ws" * V"Name")^0 * (V"ws" * P":" * Cc(":") * V"ws" * V"Name")^-1),

  varlist = Ct(Cc("varlist") * V"var" * (V"ws" * P"," * V"ws" * V"var")^0),

  -- Single variable: parsed permissively as a prefixexp, like the Lua grammar
  var = Ct(Cc("var") * V"prefixexp_inner"),

  primary = V"Name" + Ct(Cc("paren") * P"(" * V"ws" * V"exp" * V"ws" * P")"),

  var_suffix =
    Ct(Cc("index") * V"ws" * P"[" * V"ws" * V"exp" * V"ws" * P"]") +
    Ct(Cc("field") * V"ws" * P"." * V"ws" * V"Name"),

  suffix = V"var_suffix" + V"call_suffix",

  prefixexp_inner = V"primary" * V"suffix"^0,

  namelist = Ct(Cc("namelist") * V"Name" * (V"ws" * P"," * V"ws" * V"Name")^0),

  attnamelist = Ct(Cc("attnamelist") * V"Name" * V"attrib"^-1 * (V"ws" * P"," * V"ws" * V"Name" * V"attrib"^-1)^0),

  attrib = V"ws" * Ct(Cc("attrib") * P"<" * V"ws" * V"Name" * V"ws" * P">"),

  explist = Ct(Cc("explist") * V"exp" * (V"ws" * P"," * V"ws" * V"exp")^0),

  -- Expression: a flat list of operands with binop/as/is items between them
  exp = Ct(Cc("exp") * V"simple_exp" * (V"ws" * (
    V"binop" * V"ws" * V"simple_exp" +
    Ct(Cc("as") * kw"as" * V"ws" * (P"(" * V"ws" * V"typelist" * V"ws" * P")" + V"type")) +
    Ct(Cc("is") * kw"is" * V"ws" * V"type")))^0),

  simple_exp =
    kw"nil" * Ct(Cc("nil")) +
    kw"false" * Ct(Cc("boolean", false)) +
    kw"true" * Ct(Cc("boolean", true)) +
    V"Number" +
    V"String" +
    P"..." * Ct(Cc("vararg")) +
    V"functiondef" +
    V"prefixexp" +
    V"tableconstructor" +
    Ct(Cc("unop") * V"unop" * V"ws" * V"exp"),

  prefixexp = Ct(Cc("prefixexp") * V"prefixexp_inner"),

  call_suffix =
    Ct(Cc("method") * V"ws" * P":" * V"ws" * V"Name" * V"args") +
    Ct(Cc("call") * V"args"),

  functioncall = Ct(Cc("call") * V"prefixexp_inner"),

  -- Parenthesized arguments must open on the same line as the callee: Teal
  -- treats a ( on a following line as the start of a new statement
  -- (see reader.tl in the tl sources), unlike Lua's call continuation.
  args =
    S" \t"^0 * P"(" * V"ws" * (V"explist")^-1 * V"ws" * P")" +
    V"ws" * (V"tableconstructor" + V"String"),

  functiondef = Ct(Cc("function") * kw"function" * V"funcbody"),

  funcbody = Ct(Cc("funcbody") * (V"ws" * V"typeargs")^-1 * V"ws" * P"(" * V"ws" * (V"parlist")^-1 * V"ws" * P")" *
    (V"ws" * P":" * V"ws" * V"retlist")^-1 * V"ws" * V"block" * V"ws" * kw"end"),

  -- Type argument declaration: <T, U>, with optional constraint <T is Shape>
  typeargs = Ct(Cc("typeargs") * P"<" * V"ws" * V"typearg" * (V"ws" * P"," * V"ws" * V"typearg")^0 * V"ws" * P">"),

  typearg = V"Name" * (V"ws" * kw"is" * V"ws" * Ct(Cc("is") * V"type"))^-1,

  parlist = Ct(Cc("params") * (
    V"parname" * (V"ws" * P"," * V"ws" * V"parname")^0 * (V"ws" * P"," * V"ws" * V"parname_vararg")^-1 +
    V"parname_vararg")),

  parname = Ct(Cc("param") * V"Name" * (V"ws" * C(P"?"))^-1 * (V"ws" * P":" * V"ws" * V"type")^-1),

  parname_vararg = Ct(Cc("vararg") * P"..." * (V"ws" * P":" * V"ws" * V"type")^-1),

  tableconstructor = Ct(Cc("table") * P"{" * V"ws" * (V"fieldlist")^-1 * V"ws" * P"}"),

  fieldlist = Ct(Cc("fields") * V"field" * (V"fieldsep" * V"field")^0 * V"fieldsep"^-1),

  field =
    Ct(Cc("index_field") * P"[" * V"ws" * V"exp" * V"ws" * P"]" * V"ws" * P"=" * V"ws" * V"exp") +
    Ct(Cc("name_field") * V"Name" * (V"ws" * P":" * V"ws" * V"type")^-1 * V"ws" * P"=" * V"ws" * V"exp") +
    Ct(Cc("exp_field") * V"exp"),

  fieldsep = V"ws" * (P"," + P";") * V"ws",

  -- Types

  -- A type is one or more basetypes joined by | (union)
  type = Ct(Cc("type") * V"basetype" * (V"ws" * P"|" * V"ws" * V"basetype")^0),

  basetype =
    P"(" * V"ws" * V"type" * V"ws" * P")" +
    V"tabletype" +
    V"functiontype" +
    kw"nil" * Ct(Cc("nil")) +
    V"nominal",

  -- Dotted type name with optional type arguments: a.b.C<T>
  nominal = Ct(Cc("nominal") *
    C((V"ident" - V"keyword") * (P"." * (V"ident" - V"keyword"))^0) *
    (V"ws" * V"typeargs_use")^-1),

  -- Type arguments applied to a nominal: full types are allowed here, unlike
  -- the Name-only typeargs declaration form
  typeargs_use = Ct(Cc("typeargs") * P"<" * V"ws" * V"type" * (V"ws" * P"," * V"ws" * V"type")^0 * V"ws" * P">"),

  tabletype =
    Ct(Cc("map") * P"{" * V"ws" * V"type" * V"ws" * P":" * V"ws" * V"type" * V"ws" * P"}") +
    Ct(Cc("tuple") * P"{" * V"ws" * V"type" * (V"ws" * P"," * V"ws" * V"type")^1 * V"ws" * P"}") +
    Ct(Cc("array") * P"{" * V"ws" * V"type" * V"ws" * P"}"),

  -- The parameter/return section is optional so a bare `function` type works;
  -- the "()" marker records that parens were present since `function()` and
  -- `function` mean different things in Teal
  functiontype = Ct(Cc("functiontype") * kw"function" * (V"ws" * V"typeargs")^-1 *
    (V"ws" * P"(" * Cc("()") * V"ws" * (V"partypelist")^-1 * V"ws" * P")" * (V"ws" * P":" * V"ws" * V"retlist")^-1)^-1),

  partypelist = Ct(Cc("partypes") * (
    V"partype" * (V"ws" * P"," * V"ws" * V"partype")^0 * (V"ws" * P"," * V"ws" * V"partype_vararg")^-1 +
    V"partype_vararg")),

  -- Optionally named parameter type: `x: number`, `x?: number`, `? number`,
  -- `number`. A trailing ... makes the type variadic: `any...`
  partype = Ct(Cc("partype") * (
    V"Name" * (V"ws" * C(P"?"))^-1 * V"ws" * P":" * V"ws" * V"type" +
    C(P"?")^-1 * V"ws" * V"type" * C(P"...")^-1)),

  partype_vararg = Ct(Cc("vararg") * P"..." * (V"ws" * P":" * V"ws" * V"type")^-1),

  typelist = Ct(Cc("typelist") * V"type" * (V"ws" * P"," * V"ws" * V"type")^0),

  retlist = Ct(Cc("retlist") * (
    P"(" * V"ws" * (V"typelist")^-1 * (V"ws" * C(P"..."))^-1 * V"ws" * P")" +
    V"typelist" * (V"ws" * C(P"..."))^-1)),

  -- Right-hand side of a type declaration. The require form must come before
  -- the plain type form, which would otherwise match `require` as a nominal.
  newtype =
    Ct(Cc("newrecord") * kw"record" * V"recordbody") +
    Ct(Cc("newenum") * kw"enum" * V"enumbody") +
    Ct(Cc("typerequire") * kw"require" * V"ws" * P"(" * V"ws" * V"String" * V"ws" * P")" * (V"ws" * P"." * V"ws" * V"Name")^0) +
    V"type",

  recordbody = Ct(Cc("recordbody") *
    (V"ws" * V"typeargs")^-1 *
    (V"ws" * kw"is" * V"ws" * V"interfacelist")^-1 *
    (V"ws" * kw"where" * V"ws" * Ct(Cc("where") * V"exp"))^-1 *
    (V"ws" * V"recordentry")^0 *
    V"ws" * kw"end"),

  -- The first entry may be an array type (for array-like records)
  interfacelist = Ct(Cc("interfacelist") * (V"nominal" + V"tabletype") * (V"ws" * P"," * V"ws" * V"nominal")^0),

  -- Entry order matters: userdata needs a guard against a field of the same
  -- name, and the macroexp form must be tried before the plain field form
  -- would consume its function type and strand the `= macroexp` part.
  recordentry =
    Ct(Cc("userdata") * kw"userdata") * -(V"ws" * P":") +
    Ct(Cc("typedef") * kw"type" * V"ws" * V"Name" * (V"ws" * V"typeargs")^-1 * V"ws" * P"=" * V"ws" * V"newtype") +
    Ct(Cc("record") * kw"record" * V"ws" * V"Name" * V"recordbody") +
    Ct(Cc("interface") * kw"interface" * V"ws" * V"Name" * V"recordbody") +
    Ct(Cc("enum") * kw"enum" * V"ws" * V"Name" * V"enumbody") +
    V"tabletype" +
    Ct(Cc("macroexp") * V"recordkey" * V"ws" * P":" * V"ws" * V"functiontype" * V"ws" * P"=" * V"ws" * kw"macroexp" * V"macroexpbody") +
    Ct(Cc("metamethod") * kw"metamethod" * V"ws" * V"recordkey" * V"ws" * P":" * V"ws" * V"type") +
    Ct(Cc("field") * V"recordkey" * V"ws" * P":" * V"ws" * V"type"),

  recordkey = V"Name" + Ct(Cc("litkey") * P"[" * V"ws" * V"String" * V"ws" * P"]"),

  enumbody = Ct(Cc("enumbody") * (V"ws" * V"String")^0 * V"ws" * kw"end"),

  macroexpbody = Ct(Cc("macroexpbody") * (V"ws" * V"typeargs")^-1 * V"ws" * P"(" * V"ws" * (V"parlist")^-1 * V"ws" * P")" *
    (V"ws" * P":" * V"ws" * V"retlist")^-1 * V"ws" * V"retstat" * V"ws" * kw"end"),

  binop = Ct(Cc("binop") * C(
    kw"or" +
    kw"and" +
    P"<=" +
    P">=" +
    P"<<" +
    P">>" +
    P"<" +
    P">" +
    P"~=" +
    P"==" +
    P"|" +
    P"~" +
    P"&" +
    P".." +
    P"//" +
    P"/" +
    P"+" +
    P"-" +
    P"*" +
    P"%" +
    P"^"
  )),

  unop = C(P"-" + kw"not" + P"#" + P"~"),

  -- Comments (single line or multi-line with matched equals)
  comment = P"--" * (
    (P"[" * Cg(P"="^0, "eq") * P"[" * (P(1) - (P"]" * Cmb("eq") * P"]"))^0 * P"]" * Cmb("eq") * P"]") / 0 +
    -L(P"[" * P"="^0 * P"[") * (P(1) - P"\n")^0
  ),

  ws = (S" \t\n\r" + V"comment")^0,

  ident = R("az", "AZ", "__") * R("az", "AZ", "09", "__")^0,

  -- Reserved words: Lua's keywords plus as/is/global.
  -- Longer keywords must come before shorter prefixes (elseif before else)
  keyword = (
    P"and" + P"as" + P"break" + P"do" + P"elseif" + P"else" + P"end" +
    P"false" + P"for" + P"function" + P"global" + P"goto" + P"if" +
    P"in" + P"is" + P"local" + P"nil" + P"not" + P"or" + P"repeat" +
    P"return" + P"then" + P"true" + P"until" + P"while"
  ) * -idchar(),

  Name = Ct(Cc("name") * C(V"ident" - V"keyword")),

  Number = Ct(Cc("number") * C(
    (P"0" * S"xX" * R("09", "af", "AF")^1 * (P"." * R("09", "af", "AF")^0)^-1 * (S"pP" * S"+-"^-1 * R"09"^1)^-1) +
    (R"09"^1 * (P"." * R"09"^0)^-1 * (S"eE" * S"+-"^-1 * R"09"^1)^-1) +
    (P"." * R"09"^1 * (S"eE" * S"+-"^-1 * R"09"^1)^-1)
  )),

  String = Ct(Cc("string") * (
    (C(P"[" * Cg(P"="^0, "eq") * P"[" * (P(1) - (P"]" * Cmb("eq") * P"]"))^0 * P"]" * Cmb("eq") * P"]")) / 1 +
    C(P"'" * (P"\\" * P(1) + (P(1) - P"'"))^0 * P"'") +
    C(P'"' * (P"\\" * P(1) + (P(1) - P'"'))^0 * P'"')
  ))
}

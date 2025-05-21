local pgen = require "pgen"
local P, R, S, V, C, Cc, Ct, Cp = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Cc, pgen.Ct, pgen.Cp

-- Define a Lua grammar that captures into an AST
return {
  "chunk", -- initial rule name

  -- A chunk is simply a block
  chunk = Ct(Cc("chunk") * V"block" * -1),

  -- A block is a sequence of statements with an optional return statement
  block = Ct(Cc("block") * V"ws" * V"stat"^0 * V"retstat"^-1 * V"ws"),

  -- Statements
  stat = V"ws" * (
    P";" * Ct(Cc("empty")) +                                           -- empty statement
    Ct(Cc("assign") * V"varlist" * P"=" * V"explist") +              -- variable assignment
    V"functioncall" +                                                -- function call
    Ct(Cc("label") * P"::" * V"ws" * V"Name" * V"ws" * P"::") +      -- label
    Ct(Cc("break") * P"break") +                                     -- break
    Ct(Cc("goto") * P"goto" * V"ws" * V"Name") +                     -- goto
    Ct(Cc("do") * P"do" * V"block" * P"end") +                       -- do block
    Ct(Cc("while") * P"while" * V"ws" * V"exp" * V"ws" * P"do" * V"block" * P"end") +  -- while
    Ct(Cc("repeat") * P"repeat" * V"block" * P"until" * V"ws" * V"exp") +  -- repeat
    Ct(Cc("if") * P"if" * V"ws" * V"exp" * V"ws" * P"then" * V"block" *
      (P"elseif" * V"ws" * V"exp" * V"ws" * P"then" * V"block")^0 *
      (P"else" * V"block")^-1 * P"end") +                            -- if
    Ct(Cc("for_num") * P"for" * V"ws" * V"Name" * V"ws" * P"=" * V"ws" * V"exp" * P"," * V"ws" * V"exp" * 
      (P"," * V"ws" * V"exp")^-1 * V"ws" * P"do" * V"block" * P"end") + -- numeric for
    Ct(Cc("for_in") * P"for" * V"ws" * V"namelist" * V"ws" * P"in" * V"ws" * V"explist" * V"ws" * P"do" * V"block" * P"end") + -- generic for
    Ct(Cc("function") * P"function" * V"ws" * V"funcname" * V"funcbody") +  -- function definition
    Ct(Cc("local_function") * P"local" * V"ws" * P"function" * V"ws" * V"Name" * V"funcbody") +  -- local function
    Ct(Cc("local") * P"local" * V"ws" * V"attnamelist" * (P"=" * V"ws" * V"explist")^-1)  -- local variables
  ) * V"ws",

  -- Return statement
  retstat = Ct(Cc("return") * P"return" * (V"ws" * V"explist")^-1 * (V"ws" * P";")^-1),

  -- Function name
  funcname = Ct(Cc("funcname") * V"Name" * (V"ws" * P"." * V"ws" * V"Name")^0 * (V"ws" * P":" * V"ws" * V"Name")^-1),

  -- Variable list (used in assignments)
  varlist = Ct(Cc("varlist") * V"var" * (V"ws" * P"," * V"ws" * V"var")^0),

  -- Single variable (name, table index, or field access)
  var = Ct(Cc("var") * (
    V"Name" +
    Ct(Cc("index") * V"prefixexp" * V"ws" * P"[" * V"ws" * V"exp" * V"ws" * P"]") +
    Ct(Cc("field") * V"prefixexp" * V"ws" * P"." * V"ws" * V"Name")
  )),

  -- Name list (used in for loops and parameter lists)
  namelist = Ct(Cc("namelist") * V"Name" * (V"ws" * P"," * V"ws" * V"Name")^0),
  
  -- Attributed name list (for local declarations)
  attnamelist = Ct(Cc("attnamelist") * V"Name" * V"attrib"^-1 * (V"ws" * P"," * V"ws" * V"Name" * V"attrib"^-1)^0),
  
  -- Variable attribute
  attrib = V"ws" * Ct(Cc("attrib") * P"<" * V"ws" * V"Name" * V"ws" * P">"),

  -- Expression list
  explist = Ct(Cc("explist") * V"exp" * (V"ws" * P"," * V"ws" * V"exp")^0),

  -- Single expression
  exp = V"simple_exp" * (V"ws" * V"binop" * V"ws" * V"exp")^-1,

  -- Simple expression (without binary operators)
  simple_exp = 
    P"nil" * Ct(Cc("nil")) +
    P"false" * Ct(Cc("boolean", false)) +
    P"true" * Ct(Cc("boolean", true)) +
    V"Number" +
    V"String" +
    P"..." * Ct(Cc("vararg")) +
    V"functiondef" +
    V"prefixexp" +
    V"tableconstructor" +
    Ct(Cc("unop") * V"unop" * V"ws" * V"exp"),

  -- Prefix expression (variables, function calls, parenthesized expressions)
  prefixexp = 
    V"var" +
    V"functioncall" +
    Ct(Cc("paren") * P"(" * V"ws" * V"exp" * V"ws" * P")"),

  -- Function call
  functioncall = Ct(Cc("call") * 
    (V"prefixexp" * V"ws" * P":" * V"ws" * V"Name" * V"ws" * V"args" +  -- method call
     V"prefixexp" * V"ws" * V"args")                                     -- regular call
  ),

  -- Function arguments
  args = 
    P"(" * V"ws" * (V"explist")^-1 * V"ws" * P")" +
    V"tableconstructor" +
    V"String",

  -- Function definition
  functiondef = Ct(Cc("function") * P"function" * V"funcbody"),

  -- Function body (parameters and block)
  funcbody = Ct(Cc("funcbody") * V"ws" * P"(" * V"ws" * (V"parlist")^-1 * V"ws" * P")" * V"ws" * V"block" * V"ws" * P"end"),

  -- Parameter list
  parlist = Ct(Cc("params") * 
    (V"namelist" * (V"ws" * P"," * V"ws" * P"...")^-1 +  -- names with optional vararg
     P"..." * Ct(Cc("vararg")))                           -- just vararg
  ),

  -- Table constructor
  tableconstructor = Ct(Cc("table") * P"{" * V"ws" * (V"fieldlist")^-1 * V"ws" * P"}"),

  -- Field list in a table constructor
  fieldlist = Ct(Cc("fields") * V"field" * (V"fieldsep" * V"field")^0 * V"fieldsep"^-1),

  -- Individual field in a table constructor
  field = 
    Ct(Cc("index_field") * P"[" * V"ws" * V"exp" * V"ws" * P"]" * V"ws" * P"=" * V"ws" * V"exp") +
    Ct(Cc("name_field") * V"Name" * V"ws" * P"=" * V"ws" * V"exp") +
    Ct(Cc("exp_field") * V"exp"),

  -- Field separator in a table constructor
  fieldsep = V"ws" * (P"," + P";") * V"ws",

  -- Binary operators (with appropriate precedence handled by the grammar)
  binop = Ct(Cc("binop") * (
    P"or" +                                -- logical or
    P"and" +                               -- logical and
    P"<" +                                 -- less than
    P">" +                                 -- greater than
    P"<=" +                                -- less than or equal
    P">=" +                                -- greater than or equal
    P"~=" +                                -- not equal
    P"==" +                                -- equal
    P"|" +                                 -- bitwise or
    P"~" +                                 -- bitwise exclusive or
    P"&" +                                 -- bitwise and
    P"<<" +                                -- bitwise left shift
    P">>" +                                -- bitwise right shift
    P".." +                                -- string concatenation
    P"+" +                                 -- addition
    P"-" +                                 -- subtraction
    P"*" +                                 -- multiplication
    P"/" +                                 -- float division
    P"//" +                                -- floor division
    P"%" +                                 -- modulo
    P"^"                                   -- exponentiation
  ) * C(P(0))),
  
  -- Unary operators
  unop = C(P"-" + P"not" + P"#" + P"~"),

  -- Comments (single line or multi-line)
  comment = P"--" * (P"[[" * (P(1) - P"]]")^0 * P"]]" + (P(1) - P"\n")^0),

  -- Whitespace (including comments)
  ws = (S" \t\n\r" + V"comment")^0,

  -- Name (identifier)
  Name = Ct(Cc("name") * C(R("az", "AZ", "__") * R("az", "AZ", "09", "__")^0)),

  -- Number literals
  Number = Ct(Cc("number") * C(
    -- Hexadecimal number
    (P"0" * S"xX" * R("09", "af", "AF")^1 * (P"." * R("09", "af", "AF")^0)^-1 * (S"pP" * S"+-"^-1 * R"09"^1)^-1) +
    -- Decimal number
    (R"09"^1 * (P"." * R"09"^0)^-1 * (S"eE" * S"+-"^-1 * R"09"^1)^-1) +
    -- Decimal point and exponent
    (P"." * R"09"^1 * (S"eE" * S"+-"^-1 * R"09"^1)^-1)
  )),

  -- String literals
  String = Ct(Cc("string") * (
    -- Long string [[ ... ]] or [=[ ... ]=]
    C(P"[" * P"="^0 * P"[" * (P(1) - (P"]" * P"="^0 * P"]"))^0 * P"]" * P"="^0 * P"]") +
    -- Single quoted string
    C(P"'" * (P"\\" * P(1) + (P(1) - P"'"))^0 * P"'") +
    -- Double quoted string
    C(P'"' * (P"\\" * P(1) + (P(1) - P'"'))^0 * P'"')
  ))
}

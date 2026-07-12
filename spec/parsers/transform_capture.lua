local pgen = require "pgen"
local P, R, V, C, Cc, Ct, Cg, Cfn =
  pgen.P, pgen.R, pgen.V, pgen.C, pgen.Cc, pgen.Ct, pgen.Cg, pgen.Cfn

local word = R"az"^1

return {
  "test",

  test = P"1:" * V"upper" +
         P"2:" * V"multi" +
         P"3:" * V"vanish" +
         P"4:" * V"whole_match" +
         P"5:" * V"nested" +
         P"6:" * V"in_table" +
         P"7:" * V"named_value" +
         P"8:" * V"backtracked" +
         P"9:" * V"number_convert",

  -- single capture in, single value out
  upper = Cfn(C(word), [[return function(s) return s:upper() end]]),

  -- one capture in, two values out
  multi = Cfn(C(word), [[return function(s) return s, #s end]]),

  -- zero returns make the capture vanish inside a table
  vanish = Ct(C(word) * (P"," * Cfn(C(word), [[return function(s)
    if s == "skip" then return end
    return s
  end]]))^0),

  -- no inner captures: the callback receives the matched text
  whole_match = Cfn(word, [[return function(s) return "<" .. s .. ">" end]]),

  -- innermost transform runs first
  nested = Cfn(Cfn(C(word), [[return function(s) return "I:" .. s end]]),
    [[return function(s) return "O:" .. s end]]),

  -- multiple returns splice into the table's array part in order
  in_table = Ct(Cc"first" *
    Cfn(C(word), [[return function(s) return s, s:upper() end]]) *
    Cc"last"),

  -- a transform result can be a named group's value
  named_value = Ct(Cg(Cfn(C(word), [[return function(s) return s:upper() end]]), "key")),

  -- transforms in failed alternatives are never called (counted via global)
  backtracked = Ct(
    Cfn(C(word), [[return function(s)
      _G.pgen_cfn_calls = (_G.pgen_cfn_calls or 0) + 1
      return s
    end]]) * P"!" +
    C(word) * P"?"
  ),

  -- transformed values keep their Lua type
  number_convert = Cfn(C(R"09"^1), [[return function(s) return tonumber(s) + 1 end]]),
}

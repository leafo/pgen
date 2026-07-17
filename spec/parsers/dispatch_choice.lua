local pgen = require "pgen"
local P, V, C, Cc, Cmt, Cfn, T =
  pgen.P, pgen.V, pgen.C, pgen.Cc, pgen.Cmt, pgen.Cfn, pgen.T

return {
  "test",

  test = P"1:" * V"disjoint" +
    P"2:" * V"overlap" +
    P"3:" * V"unknown" +
    P"4:" * V"nullable" +
    P"5:" * V"labeled" +
    P"6:" * V"transform" +
    P"7:" * V"at_eof" +
    P"8:" * V"trailing" +
    P"9:" * V"recursive",

  -- Structured alternatives with disjoint initial bytes.
  disjoint = C(P"apple" * P"!") + C(P"banana") +
    C(P"cherry") + C(P"date"),

  -- Alternatives sharing a prefix must retain their original PEG order.
  overlap = C(P"for" * P"x") + C(P"for") + C(P"foo") + C(P"bar"),

  -- Cmt has an unknown FIRST set and therefore must be tried for every byte.
  unknown = Cmt(Cc"dynamic", [[local subject, pos, value = ...
    if subject:sub(pos, pos) == "x" then
      return pos, value
    end
    return false
  ]]) + C(P"a") + C(P"b") + C(P"c"),

  -- A nullable alternative remains a candidate regardless of the next byte.
  nullable = P"x" * P"!" + Cc"empty" + P"a" + P"b",

  -- A label may fire before a terminal is inspected and cannot be skipped.
  labeled = T"boom" + P"a" + P"b" + P"c",

  -- Optimizer-generated nodes must remain traversable so Cfn callbacks are
  -- still collected and assigned an id by the generator.
  transform = Cfn(P"a", [[return function(value)
    return value:upper()
  end]]) + C(P"b") + C(P"c") + C(P"d"),

  -- EOF uses a separate candidate mask containing nullable alternatives.
  -- The literals are wrapped in C() so the trie pass doesn't collapse the
  -- choice before the dispatch pass sees it.
  at_eof = C(P"a") + C(P"b") + C(P"c") + Cc"eof",

  -- A dispatched choice followed by a failing pattern, for comparing error
  -- reporting (furthest position, --pgen-errors message) against the
  -- unoptimized parser.
  trailing = (C(P"aa") + C(P"bb") + C(P"cc") + C(P"d")) * P"Z",

  -- An alternative with a provably empty FIRST set (left recursion) must not
  -- be eliminated: the dispatched parser has to hit the same recursion-depth
  -- error the unoptimized parser reports.
  recursive = C(V"lrec") + C(P"bb") + C(P"cc") + C(P"dd"),
  lrec = V"lrec" * P"x",
}

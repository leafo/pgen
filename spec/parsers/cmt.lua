local pgen = require "pgen"
local P, C, Ct, Cmt, V, R, S = pgen.P, pgen.C, pgen.Ct, pgen.Cmt, pgen.V, pgen.R, pgen.S

-- Test grammar for Cmt (match-time capture)
return {
  "test",

  -- Main dispatcher: prefix selects which test to run
  test = P"1:" * V"return_pos" +
         P"2:" * V"return_true" +
         P"3:" * V"return_false" +
         P"4:" * V"return_nil" +
         P"5:" * V"return_extra_captures" +
         P"6:" * V"captures_passed" +
         P"7:" * V"inside_ct" +
         P"8:" * V"skip_chars" +
         P"9:" * V"no_return",

  -- Test 1: Cmt that returns position (advances by consuming matched text)
  return_pos = Cmt(P"hello", [[
    local subject, pos = ...
    return pos  -- just return the position after "hello"
  ]]),

  -- Test 2: Cmt that returns true (succeed without consuming extra)
  return_true = Cmt(P"hello", [[
    local subject, pos = ...
    return true  -- succeed, position stays after "hello"
  ]]),

  -- Test 3: Cmt that returns false (fail)
  return_false = Cmt(P"hello", [[
    local subject, pos = ...
    return false  -- fail the match
  ]]),

  -- Test 4: Cmt that returns nil (fail)
  return_nil = Cmt(P"hello", [[
    local subject, pos = ...
    return nil  -- fail the match
  ]]),

  -- Test 5: Cmt that produces extra captures
  return_extra_captures = Cmt(P"abc", [[
    local subject, pos = ...
    return pos, "first", "second", 123
  ]]),

  -- Test 6: Cmt receives captures from inner pattern
  captures_passed = Cmt(C(P"foo") * C(P"bar"), [[
    local subject, pos, cap1, cap2 = ...
    if cap1 == "foo" and cap2 == "bar" then
      return pos, cap1 .. "-" .. cap2  -- combine captures
    end
    return false
  ]]),

  -- Test 7: Cmt inside Ct collects its captures
  inside_ct = Ct(Cmt(P"xyz", [[
    local subject, pos = ...
    return pos, "cap1", "cap2"
  ]])),

  -- Test 8: Cmt that consumes additional characters
  skip_chars = Cmt(P"start", [[
    local subject, pos = ...
    -- Skip any following spaces
    while pos <= #subject and subject:sub(pos, pos) == " " do
      pos = pos + 1
    end
    return pos
  ]]) * P"end",

  -- Test 9: Cmt with no return (should fail)
  no_return = Cmt(P"test", [[
    local subject, pos = ...
    -- no return statement
  ]]),
}

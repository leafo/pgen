-- Furthest-failure position reporting: on ordinary (unlabeled) parse
-- failure, parse() returns nil, message|nil, furthest_pos where
-- furthest_pos is the deepest input position (1-indexed) at which any
-- match attempt failed. Labeled failures (T) keep their own position.

describe("furthest failure position", function()
  local pgen = require "pgen"
  local parser = pgen.require("spec.parsers.furthest")

  local function fail_pos(input)
    local result, err, pos = parser.parse(input)
    assert.is_nil(result)
    assert.is_nil(err) -- no PGEN_ERRORS in on-the-fly builds
    return pos
  end

  it("returns no extra values on success", function()
    assert.same({"x", "1"}, {parser.parse("1:x = 1")})
  end)

  it("reports position of a bad token, not the statement start", function()
    -- "1:x = @" -> value expected at position 7
    assert.equal(7, fail_pos("1:x = @"))
  end)

  it("reports the deepest statement in a sequence", function()
    -- second statement has the error at its value
    --         123456789012345
    assert.equal(15, fail_pos("1:x = 1; yy = @"))
  end)

  it("reports position past a partially matched value", function()
    -- "func(" matches P"func" then fails inside "()"
    -- pos of "func" start is 7; the literal "()" attempt fails at 11
    assert.equal(11, fail_pos("1:x = func(;"))
  end)

  it("reports unterminated string at end of input", function()
    --         12345678901
    assert.equal(11, fail_pos("1:x = 'abc"))
  end)

  it("reports trailing garbage after a valid prefix", function()
    -- statement parses, ws consumes the space, then ";" and EOF both fail
    -- at the first "!"
    --         123456789
    assert.equal(9, fail_pos("1:x = 1 !!!"))
  end)

  it("is not dominated by failures inside successful predicates", function()
    -- L(P"ab") succeeds; P"abc" then fails. The reported position comes
    -- from the failed P"abc" attempt, not from inside the lookahead
    local pos = fail_pos("2:abX")
    assert.equal(3, pos)
  end)

  it("reports negate failures at the forbidden pattern position", function()
    -- P"x" * -P"y": fails the negation at the "y" (position 4 after "3:x")
    assert.equal(4, fail_pos("3:xy"))
  end)

  it("reports Cmt rejection at the position after the inner pattern", function()
    -- Cmt inner matches "999" (rejected by callback since > 100);
    -- record lands after the digits
    --                       123456789
    assert.equal(9, fail_pos("4:aaa999"))
  end)

  it("keeps labeled failure reporting unchanged", function()
    -- t_test grammar: label position comes from throw_pos, not furthest
    local t_parser = pgen.require("spec.parsers.t_test")
    local result, label, pos = t_parser.parse("2:nomatch")
    assert.is_nil(result)
    assert.equal("expected_match", label)
    assert.equal(3, pos)
  end)

  it("resets the furthest position between parses", function()
    assert.equal(15, fail_pos("1:x = 1; yy = @"))
    assert.equal(7, fail_pos("1:x = @"))
  end)
end)

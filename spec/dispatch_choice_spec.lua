local pgen = require "pgen"

describe("FIRST-byte choice dispatch", function()
  local parser

  setup(function()
    parser = pgen.require("spec.parsers.dispatch_choice")
  end)

  it("matches structured alternatives with disjoint initial bytes", function()
    assert.same({"apple!"}, {parser.parse("1:apple!")})
    assert.same({"banana"}, {parser.parse("1:banana")})
    assert.same({"cherry"}, {parser.parse("1:cherry")})
    assert.same({"date"}, {parser.parse("1:date")})
  end)

  it("preserves order for overlapping prefixes", function()
    assert.same({"forx"}, {parser.parse("2:forx")})
    assert.same({"for"}, {parser.parse("2:for")})
    assert.same({"foo"}, {parser.parse("2:foo")})
  end)

  it("retains unknown and nullable alternatives in every bucket", function()
    assert.same({"dynamic"}, {parser.parse("3:x")})
    assert.same({"a"}, {parser.parse("3:a")})
    assert.same({"empty"}, {parser.parse("4:x")})
  end)

  it("does not skip labeled failures", function()
    local result, label = parser.parse("5:a")
    assert.is_nil(result)
    assert.same("boom", label)
  end)

  it("keeps transform callbacks reachable through optimized nodes", function()
    assert.same({"A"}, {parser.parse("6:a")})
  end)

  it("uses the nullable candidate mask at end of input", function()
    assert.same({"eof"}, {parser.parse("7:")})
  end)

  it("does not eliminate alternatives with an empty FIRST set", function()
    -- the left-recursive alternative must run and abort with the same
    -- recursion-depth error the unoptimized parser reports
    local ok, err = pcall(parser.parse, "9:bb")
    assert.is_false(ok)
    assert.matches("max recursion depth", err)
  end)
end)

describe("FIRST-byte choice dispatch error reporting parity", function()
  local optimized, unoptimized

  setup(function()
    optimized = pgen.require("spec.parsers.dispatch_choice",
      {pgen_errors = true})
    unoptimized = pgen.require("spec.parsers.dispatch_choice",
      {pgen_errors = true, optimize = false})
  end)

  local function assert_same_failure(input)
    local result, message, position = optimized.parse(input)
    local expected_result, expected_message, expected_position =
      unoptimized.parse(input)
    assert.is_nil(result)
    assert.is_nil(expected_result)
    assert.same(expected_message, message)
    assert.same(expected_position, position)
  end

  it("reports the same failure when a pattern after the choice fails", function()
    -- alternative `d` matches but the following `Z` fails; the alternatives
    -- skipped by dispatch must still contribute their failure position
    assert_same_failure("8:d?")
  end)

  it("reports the same failure when every alternative is skipped", function()
    assert_same_failure("8:zz")
  end)

  it("reports the same failure at end of input", function()
    assert_same_failure("8:")
  end)
end)

describe("FIRST-byte choice dispatch optimization", function()
  it("emits a dispatcher for a wide structured choice", function()
    local P, C = pgen.P, pgen.C
    local grammar = {
      "test",
      test = C(P"a" * P"1") + C(P"b" * P"2") +
        C(P"c" * P"3") + C(P"d" * P"4"),
    }
    local code = pgen.compile(grammar, {parser_name = "dispatch_test"})
    assert.matches("FIRST%-byte dispatched ordered choice", code)
  end)

  it("can be disabled with optimize=false", function()
    local P, C = pgen.P, pgen.C
    local grammar = {
      "test",
      test = C(P"a" * P"1") + C(P"b" * P"2") +
        C(P"c" * P"3") + C(P"d" * P"4"),
    }
    local code = pgen.compile(grammar, {
      parser_name = "dispatch_test",
      optimize = false,
    })
    assert.falsy(code:match("FIRST%-byte dispatched ordered choice"))
  end)
end)

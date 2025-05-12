local parser = require("calc_parser")

describe("calc_parser", function()
  local function parse_and_assert(input, expected_position)
    assert.are.equal(expected_position, parser.parse(input))
  end

  it("parses basic integer", function()
    parse_and_assert("5", 2)
    parse_and_assert("123", 4)
  end)

  it("parses float", function()
    parse_and_assert("3.14", 5)
  end)

  it("parses addition", function()
    parse_and_assert("2 + 3", 6)
  end)

  it("parses multiplication", function()
    parse_and_assert("4 * 5", 6)
  end)

  it("parses complex expression", function()
    parse_and_assert("2 * (3 + 4)", 12)
  end)

  it("handles whitespace correctly", function()
    parse_and_assert("  123  ", 8)
    parse_and_assert("  10  +  5  ", 13)
  end)

  it("parses nested parentheses", function()
    parse_and_assert("((2 + 3) * (4 + 5))", 20)
  end)

  it("parses negative numbers", function()
    parse_and_assert("-7", 3)
  end)

  it("parses division", function()
    parse_and_assert("10 / 2", 7)
  end)

  it("parses mixed operations", function()
    parse_and_assert("5 + 3 * 2", 10)
  end)

  it("returns nil for invalid input", function()
    assert.is_nil(parser.parse("3 + * 4"))
  end)
end)


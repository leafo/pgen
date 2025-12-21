describe("capture group (Cg) parser", function()
  local pgen = require "pgen"
  local parser

  before_each(function()
    parser = pgen.require("spec.parsers.capture_group")
  end)

  it("creates named fields in Ct", function()
    local result = parser.parse("1:hello world")
    assert.same({
      greeting = "hello",
      "world"
    }, result)
  end)

  it("handles multiple named captures", function()
    local result = parser.parse("2:ab")
    assert.same({
      first = "a",
      second = "b"
    }, result)
  end)

  it("handles mixed named and positional captures", function()
    local result = parser.parse("3:xyz")
    assert.same({
      "x",
      named = "y",
      "z"
    }, result)
  end)

  it("works with Cp position captures", function()
    local result = parser.parse("4:hello")
    assert.same({
      pos = 3, -- position after "4:"
      "hello"
    }, result)
  end)

  it("works with Cc constant captures", function()
    local result = parser.parse("5:value")
    assert.same({
      "type",
      data = "value"
    }, result)
  end)

  it("uses only first capture when Cg contains multiple captures", function()
    local result = parser.parse("6:hi there")
    assert.same({
      greeting = "hi"
    }, result)
  end)
end)

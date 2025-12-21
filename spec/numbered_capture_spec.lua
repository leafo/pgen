describe("numbered capture (patt/n) parser", function()
  local pgen = require "pgen"
  local parser

  before_each(function()
    parser = pgen.require("spec.parsers.numbered_capture")
  end)

  it("selects first capture with /1", function()
    local result = parser.parse("1:abc")
    assert.equal("a", result)
  end)

  it("selects second capture with /2", function()
    local result = parser.parse("2:abc")
    assert.equal("b", result)
  end)

  it("selects third capture with /3", function()
    local result = parser.parse("3:abc")
    assert.equal("c", result)
  end)

  it("discards all captures with /0", function()
    local result = parser.parse("4:hello world")
    -- /0 produces no captures, so the parser returns the position
    assert.is_number(result)
  end)

  it("returns nil when n out of range", function()
    local result = parser.parse("5:ab")
    assert.is_nil(result)
  end)

  it("works with single capture", function()
    local result = parser.parse("6:single")
    assert.equal("single", result)
  end)

  it("works inside Ct", function()
    local result = parser.parse("7:xyz")
    assert.same({"x", "z"}, result)
  end)

  it("works with nested numbered captures", function()
    local result = parser.parse("8:abc")
    -- Inner /2 selects "b", outer /1 selects "b" (only one capture at that point)
    assert.equal("b", result)
  end)
end)

describe("numbered capture construction errors", function()
  local pgen = require "pgen"

  it("rejects negative numbers", function()
    assert.has_error(function()
      local _ = pgen.C("test") / -1
    end, "Cn index must be non-negative")
  end)

  it("rejects non-integer numbers", function()
    assert.has_error(function()
      local _ = pgen.C("test") / 1.5
    end, "Cn index must be an integer")
  end)

  it("rejects non-number divisors", function()
    assert.has_error(function()
      local _ = pgen.C("test") / "string"
    end, "Pattern division only supports number argument (numbered capture)")
  end)
end)

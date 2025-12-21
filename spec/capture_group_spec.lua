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

  it("works with Cc constant capture as Cg pattern", function()
    local result = parser.parse("4:hello")
    assert.same({
      const_field = "constant_value",
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

  describe("Cg outside Ct", function()
    it("returns only position when Cg is alone (sentinel stripped)", function()
      -- Cg(C(P"hello"), "name") - Cg captures should be invisible outside Ct
      local result = parser.parse("7:hello")
      assert.equals(8, result)  -- position after "7:hello" (7 chars, position 8)
    end)

    it("returns only regular captures when mixed with Cg", function()
      -- C(P"hello") * Cg(Cc("named"), "name") * C(P"world")
      local r1, r2, r3 = parser.parse("8:helloworld")
      assert.equals("hello", r1)
      assert.equals("world", r2)
      assert.is_nil(r3)  -- no third value, Cg is stripped
    end)

    it("returns only position when Cg with Cc (no other captures)", function()
      -- P"x" * Cg(Cc("value"), "key") * P"y" - no captures except Cg which is stripped
      local result = parser.parse("9:xy")
      assert.equals(5, result)  -- position after "9:xy" (4 chars + 1 = 5)
    end)
  end)
end)

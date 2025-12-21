describe("repeat class optimization", function()
  local pgen = require "pgen"
  local parser

  before_each(function()
    parser = pgen.require("spec.parsers.repeat_class")
  end)

  describe("set with max=1", function()
    it("accepts zero or one", function()
      assert.same({ "X" }, { parser.parse("1:X") })
      assert.same({ "X" }, { parser.parse("1:+X") })
      assert.same({ "X" }, { parser.parse("1:-X") })
    end)

    it("rejects more than one", function()
      assert.is_nil(parser.parse("1:++X"))
      assert.is_nil(parser.parse("1:+-X"))
    end)
  end)

  describe("range with max=2", function()
    it("accepts up to two digits", function()
      assert.same({ "X" }, { parser.parse("2:X") })
      assert.same({ "X" }, { parser.parse("2:1X") })
      assert.same({ "X" }, { parser.parse("2:12X") })
    end)

    it("rejects three digits", function()
      assert.is_nil(parser.parse("2:123X"))
    end)
  end)

  describe("char with max=1", function()
    it("accepts zero or one char", function()
      assert.same({ "X" }, { parser.parse("3:X") })
      assert.same({ "X" }, { parser.parse("3:=X") })
    end)

    it("rejects more than one char", function()
      assert.is_nil(parser.parse("3:==X"))
    end)
  end)
end)

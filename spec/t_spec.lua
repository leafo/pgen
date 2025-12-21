describe("labeled failure (T) parser", function()
  local pgen = require "pgen"
  local parser

  before_each(function()
    parser = pgen.require("spec.parsers.t_test")
  end)

  describe("basic throw", function()
    it("throws labeled failure immediately", function()
      local result, label, pos = parser.parse("1:anything")
      assert.is_nil(result)
      assert.equal("always_fails", label)
      assert.equal(3, pos)  -- Position after "1:"
    end)
  end)

  describe("choice with throw", function()
    it("returns normal result when pattern matches", function()
      local result = parser.parse("2:match")
      assert.equal(8, result)  -- Position after full match
    end)

    it("throws when no alternative matches", function()
      local result, label, pos = parser.parse("2:nomatch")
      assert.is_nil(result)
      assert.equal("expected_match", label)
      assert.equal(3, pos)  -- Position after "2:"
    end)
  end)

  describe("conditional throw", function()
    it("succeeds with single identifier", function()
      local id1 = parser.parse("3:foo")
      assert.equal("foo", id1)
    end)

    it("succeeds with valid identifier list", function()
      local id1, id2 = parser.parse("3:foo,bar")
      assert.equal("foo", id1)
      assert.equal("bar", id2)
    end)

    it("succeeds with longer list", function()
      local id1, id2, id3 = parser.parse("3:one,two,three")
      assert.equal("one", id1)
      assert.equal("two", id2)
      assert.equal("three", id3)
    end)

    it("throws on missing identifier after comma", function()
      local result, label, pos = parser.parse("3:foo,123")
      assert.is_nil(result)
      assert.equal("expected_identifier", label)
      assert.equal(7, pos)  -- Position after "3:foo,"
    end)

    it("throws on trailing comma", function()
      local result, label, pos = parser.parse("3:foo,")
      assert.is_nil(result)
      assert.equal("expected_identifier", label)
      assert.equal(7, pos)  -- Position at end
    end)
  end)

  describe("throw after capture", function()
    it("succeeds when full pattern matches", function()
      local cap = parser.parse("4:hello!")
      assert.equal("hello", cap)
    end)

    it("throws after capturing when remainder fails", function()
      local result, label, pos = parser.parse("4:hello?")
      assert.is_nil(result)
      assert.equal("expected_exclamation", label)
      assert.equal(8, pos)  -- Position after "4:hello"
    end)
  end)

  describe("predicate swallow", function()
    it("does not propagate labeled failure inside negation", function()
      local result, label, pos = parser.parse("5:ok")
      assert.equal(5, result)  -- Position after "5:ok"
      assert.is_nil(label)
      assert.is_nil(pos)
    end)
  end)
end)

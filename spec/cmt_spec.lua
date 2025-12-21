local pgen = require "pgen"

describe("cmt (match-time capture)", function()
  local parser

  before_each(function()
    parser = pgen.require("spec.parsers.cmt")
  end)

  describe("return position", function()
    it("succeeds when returning position", function()
      -- "1:hello" = 7 chars, position 8 means all consumed
      assert.same({8}, {parser.parse("1:hello")})
    end)

    it("fails when inner pattern doesn't match", function()
      local result = parser.parse("1:world")
      assert.is_nil(result)
    end)
  end)

  describe("return true", function()
    it("succeeds when returning true", function()
      -- "2:hello" = 7 chars, position 8 means all consumed
      assert.same({8}, {parser.parse("2:hello")})
    end)
  end)

  describe("return false", function()
    it("fails when returning false", function()
      local result = parser.parse("3:hello")
      assert.is_nil(result)
    end)
  end)

  describe("return nil", function()
    it("fails when returning nil", function()
      local result = parser.parse("4:hello")
      assert.is_nil(result)
    end)
  end)

  describe("extra captures", function()
    it("returns extra return values as captures", function()
      local results = {parser.parse("5:abc")}
      assert.same({"first", "second", 123}, results)
    end)
  end)

  describe("captures passed to callback", function()
    it("receives inner captures and can transform them", function()
      local results = {parser.parse("6:foobar")}
      assert.same({"foo-bar"}, results)
    end)
  end)

  describe("inside Ct", function()
    it("collects Cmt captures into table", function()
      local results = {parser.parse("7:xyz")}
      assert.same({{"cap1", "cap2"}}, results)
    end)
  end)

  describe("skip chars (advance position)", function()
    it("can skip whitespace and continue matching", function()
      -- "8:start   end" = 13 chars, position 14 means all consumed
      assert.same({14}, {parser.parse("8:start   end")})
    end)

    it("works with no whitespace to skip", function()
      -- "8:startend" = 10 chars, position 11 means all consumed
      assert.same({11}, {parser.parse("8:startend")})
    end)

    it("fails if following pattern doesn't match", function()
      local result = parser.parse("8:start   notend")
      assert.is_nil(result)
    end)
  end)

  describe("no return", function()
    it("fails when callback doesn't return anything", function()
      local result = parser.parse("9:test")
      assert.is_nil(result)
    end)
  end)
end)

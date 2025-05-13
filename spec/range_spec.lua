local pgen = require "pgen"

describe("multiple ranges", function()
  local parser = pgen.require("spec.parsers.range")

  it("parses digits", function()
    local res = parser.parse("123")
    assert.same({"123"}, res)
  end)

  it("parses lowercase letters", function()
    local res = parser.parse("abc")
    assert.same({"abc"}, res)
  end)

  it("parses uppercase letters", function()
    local res = parser.parse("XYZ")
    assert.same({"XYZ"}, res)
  end)

  it("parses mixed alphanumeric characters", function()
    local res = parser.parse("abc123XYZ")
    assert.same({"abc123XYZ"}, res)
  end)

  it("parses multiple items", function()
    local res = parser.parse("abc 123 XYZ")
    assert.same({"abc", "123", "XYZ"}, res)
  end)

  it("fails on characters outside the ranges", function()
    local res, err = parser.parse("abc!123")
    assert.is_nil(res)
    assert.is_not_nil(err)
  end)
end)

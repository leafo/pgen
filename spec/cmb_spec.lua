describe("capture match back (Cmb) parser", function()
  local pgen = require "pgen"
  local parser

  before_each(function()
    parser = pgen.require("spec.parsers.cmb")
  end)

  it("matches basic backreference", function()
    local pos = parser.parse("1:aaa:aaa")
    assert.truthy(pos)
  end)

  it("fails when backreference doesn't match", function()
    local result = parser.parse("1:aaa:aa")
    assert.is_nil(result)
  end)

  it("parses lua long strings", function()
    local result = parser.parse("2:[==[hello world]==]")
    assert.same({eq = "==", "hello world"}, result)
  end)

  it("parses lua long strings with no equals", function()
    local result = parser.parse("2:[[simple]]")
    assert.same({eq = "", "simple"}, result)
  end)

  it("fails mismatched lua long string delimiters", function()
    local result = parser.parse("2:[==[wrong]=]")
    assert.is_nil(result)
  end)

  it("matches empty capture", function()
    local result = parser.parse("3:done")
    assert.same({maybe = "", "done"}, result)
  end)
end)

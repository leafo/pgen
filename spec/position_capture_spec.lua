-- Use the compiled parser

describe("position_capture", function()
  local pgen = require "pgen"
  local parser = pgen.require("spec.parsers.position_capture")

  it("captures positions correctly", function()
    local result = parser.parse("a, b, c,")
    
    assert.same({
      {1, "a"},
      {4, "b"},
      {7, "c"},
    }, result)
  end)
  
  it("handles empty input correctly", function()
    local result = parser.parse("")
    assert.is_table(result)
    assert.equals(0, #result)
  end)
  
  it("handles whitespace correctly", function()
    local result = parser.parse("  x  ,  y  ,  ")

    assert.same({
      {3, "x"},
      {9, "y"},
    }, result)
  end)
end)

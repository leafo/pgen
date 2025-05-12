describe("constant capture parser", function()
  local pgen = require "pgen"
  
  it("correctly parses and adds constant values", function()
    local parser = pgen.require("spec.parsers.constant_capture")
    
    local result, err = parser.parse("name = \"John\"; age = 30; active = true;")
    assert.same({
      {
        "name", "John", "pair" -- Key, value, and constant type
      },
      {
        "age", "30", "pair"
      },
      {
        "active", "true", "pair"
      }
    }, result)
    
    -- Test with different constant types
    local parser2 = pgen.require("spec.parsers.multiple_constants")
    
    local result2 = { assert(parser2.parse("test")) }
    assert.same({
      42, -- number
      "test_field", -- string
      true, -- boolean
      nil -- nil
    }, result2)
  end)
end)

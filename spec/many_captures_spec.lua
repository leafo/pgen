local pgen = require "pgen"

describe("capture stack growth", function()
  local parser

  setup(function()
    parser = pgen.require("spec.parsers.many_captures")
  end)

  it("returns many captures as multiple values", function()
    local count = select("#", parser.parse("1:" .. ("a"):rep(100)))
    assert.same(100, count)
  end)

  it("collects many captures into a table", function()
    local result = parser.parse("2:" .. ("a"):rep(1000))
    assert.same(1000, #result)
  end)

  -- Captures live on the Lua stack during the parse, so total simultaneous
  -- captures are bounded by LUAI_MAXCSTACK (8000 in stock Lua 5.1). These
  -- assert the overflow is a clean Lua error instead of undefined behavior.

  it("raises a clean error when captures exceed the Lua stack limit", function()
    local ok, err = pcall(parser.parse, "1:" .. ("a"):rep(20000))
    assert.is_false(ok)
    assert.matches("stack overflow", err)
  end)

  it("raises a clean error inside a capture table as well", function()
    local ok, err = pcall(parser.parse, "2:" .. ("a"):rep(20000))
    assert.is_false(ok)
    assert.matches("stack overflow", err)
  end)
end)

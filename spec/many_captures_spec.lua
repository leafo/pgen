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

  -- Captures are recorded in a C-side log during the parse and only
  -- materialized onto the Lua stack afterwards, so only the number of
  -- top-level return values is bounded by the runtime's Lua stack limit.
  -- Lua 5.1 and LuaJIT reject 20000 values, while newer Lua versions have
  -- a larger limit and can return them all. Captures inside a table are
  -- materialized one at a time and do not hit this limit.

  it("returns all captures or raises a clean error at the Lua stack limit", function()
    local ok, result = pcall(function()
      return select("#", parser.parse("1:" .. ("a"):rep(20000)))
    end)

    if ok then
      assert.same(20000, result)
    else
      assert.matches("stack overflow", result)
    end
  end)

  it("collects captures beyond the stack limit inside a capture table", function()
    local result = parser.parse("2:" .. ("a"):rep(20000))
    assert.same(20000, #result)
  end)
end)

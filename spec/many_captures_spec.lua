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
  -- top-level return values is bounded by LUAI_MAXCSTACK (8000 in stock
  -- Lua 5.1). Overflowing that is a clean Lua error, and captures inside
  -- a table are unbounded (materialized one at a time).

  it("raises a clean error when return values exceed the Lua stack limit", function()
    local ok, err = pcall(parser.parse, "1:" .. ("a"):rep(20000))
    assert.is_false(ok)
    assert.matches("stack overflow", err)
  end)

  it("collects captures beyond the stack limit inside a capture table", function()
    local result = parser.parse("2:" .. ("a"):rep(20000))
    assert.same(20000, #result)
  end)
end)

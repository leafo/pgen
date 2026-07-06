local pgen = require "pgen"

describe("recursion depth limit", function()
  local parser

  setup(function()
    parser = pgen.require("spec.parsers.recursion", {max_depth = 100})
  end)

  it("parses nesting below the limit", function()
    local input = ("("):rep(50) .. "x" .. (")"):rep(50)
    assert.same("x", parser.parse(input))
  end)

  it("raises a clean Lua error past the depth limit", function()
    local input = ("("):rep(200) .. "x" .. (")"):rep(200)
    local ok, err = pcall(parser.parse, input)
    assert.is_false(ok)
    assert.matches("max recursion depth", err)
  end)

  it("can still parse after a depth failure", function()
    local deep = ("("):rep(200) .. "x" .. (")"):rep(200)
    pcall(parser.parse, deep)
    assert.same("x", parser.parse("(x)"))
  end)
end)

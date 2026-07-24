-- The Lua code generation target (target = "lua") is exercised explicitly
-- here so it gets coverage even when the rest of the suite runs against the
-- default C target. The full suite can be run against the Lua target with
-- `make busted-lua` (PGEN_TARGET=lua).
local pgen = require "pgen"

describe("lua target", function()
  local P, R, S, V, C, Cc, Ct, Cg, T =
    pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Cc, pgen.Ct, pgen.Cg, pgen.T

  local function compile_and_load(grammar, options)
    options = options or {}
    options.target = "lua"
    options.parser_name = options.parser_name or "lua_target_test"
    local code = pgen.compile(grammar, options)
    -- a leftover placeholder means a template variable was never filled in
    assert.falsy(code:match("%$[A-Z_]+%$"))
    local load_chunk = loadstring or load
    local chunk, err = load_chunk(code, "lua_target_test")
    assert.truthy(chunk, err)
    return chunk(), code
  end

  it("generates a loadable pure Lua module", function()
    local parser, code = compile_and_load{
      "start",
      start = Ct(C(P"hello") * S" \t"^1 * C(R"az"^1)),
    }
    -- no C artifacts in the output
    assert.falsy(code:match("#include"))
    assert.same({{"hello", "world"}}, {parser.parse("hello world")})
    assert.is_nil(parser.parse("goodbye"))
  end)

  it("returns the consumed position when there are no captures", function()
    local parser = compile_and_load{
      "start",
      start = P"abc" * P"d"^-1,
    }
    assert.same(5, parser.parse("abcd"))
    assert.same(4, parser.parse("abc"))
  end)

  it("supports labeled failures", function()
    local parser = compile_and_load{
      "start",
      start = P"a" * (P"b" + T"expected_b"),
    }
    local result, label, pos = parser.parse("ax")
    assert.is_nil(result)
    assert.same("expected_b", label)
    assert.same(2, pos)
  end)

  it("supports named groups and constants inside tables", function()
    local parser = compile_and_load{
      "start",
      start = Ct(Cg(C(R"09"^1), "num") * Cc("tagged", true)),
    }
    local result = parser.parse("42")
    assert.same({"tagged", true, num = "42"}, result)
  end)

  it("honors the max_depth option", function()
    local parser = compile_and_load({
      "start",
      start = P"(" * V"start" * P")" + P"x",
    }, {max_depth = 50})
    assert.same(4, parser.parse("(x)"))
    local ok, err = pcall(parser.parse, ("("):rep(100) .. "x" .. (")"):rep(100))
    assert.is_false(ok)
    assert.matches("max recursion depth", err)
  end)

  it("generates error messages with pgen_errors", function()
    local parser = compile_and_load({
      "start",
      start = P"hello",
    }, {pgen_errors = true})
    local result, message = parser.parse("help")
    assert.is_nil(result)
    assert.matches("Expected `hello`", message)
  end)

  it("floors fractional positions returned by Cmt callbacks", function()
    local parser = compile_and_load{
      "start",
      start = pgen.Cmt(P"ab", [[
        local subject, pos = ...
        return pos + 1.5
      ]]) * C(P(1)),
    }
    -- pos 3 + 1.5 floors to 4: one extra character consumed by the callback
    assert.same({"d"}, {parser.parse("abcd")})
  end)

  it("loads through pgen.require with target option", function()
    local parser = pgen.require("spec.parsers.json_parser", {target = "lua"})
    local result = parser.parse('[1, 2]')
    assert.same("json", result[1])
    assert.same("array", result[2][1])
  end)
end)

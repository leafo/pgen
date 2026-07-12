describe("transform captures", function()
  local pgen = require "pgen"
  local parser

  setup(function()
    parser = pgen.require("spec.parsers.transform_capture")
  end)

  it("transforms a single capture", function()
    assert.same("ABC", parser.parse("1:abc"))
  end)

  it("returns multiple values", function()
    assert.same({"abc", 3}, {parser.parse("2:abc")})
  end)

  it("drops captures when the callback returns nothing", function()
    assert.same({"a", "b"}, parser.parse("3:a,skip,b"))
    assert.same({"a", "x", "b"}, parser.parse("3:a,x,b"))
  end)

  it("passes the matched text when there are no captures", function()
    assert.same("<abc>", parser.parse("4:abc"))
  end)

  it("evaluates nested transforms innermost first", function()
    assert.same("O:I:abc", parser.parse("5:abc"))
  end)

  it("splices multiple returns into a capture table", function()
    assert.same({"first", "abc", "ABC", "last"}, parser.parse("6:abc"))
  end)

  it("provides transformed values to named groups", function()
    assert.same({key = "ABC"}, parser.parse("7:abc"))
  end)

  it("never calls transforms in backtracked alternatives", function()
    _G.pgen_cfn_calls = nil
    assert.same({"abc"}, parser.parse("8:abc?"))
    assert.is_nil(_G.pgen_cfn_calls)

    assert.same({"abc"}, parser.parse("8:abc!"))
    assert.same(1, _G.pgen_cfn_calls)
    _G.pgen_cfn_calls = nil
  end)

  it("transformed values keep their Lua type", function()
    assert.same(43, parser.parse("9:42"))
  end)

  it("raises a load error when the chunk does not return a function", function()
    local ok, err = pcall(pgen.require, "spec.parsers.transform_bad")
    assert.is_false(ok)
    assert.matches("did not return a function", tostring(err))
  end)
end)

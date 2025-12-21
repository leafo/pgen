local pgen = require "pgen"

describe("trie optimization", function()
  local parser

  before_each(function()
    parser = pgen.require("spec.parsers.trie_optimization")
  end)

  describe("basic keyword matching (test1)", function()
    it("matches first keyword", function()
      assert.same({"foo"}, {parser.parse("1:foo")})
    end)

    it("matches middle keywords", function()
      assert.same({"bar"}, {parser.parse("1:bar")})
      assert.same({"baz"}, {parser.parse("1:baz")})
    end)

    it("matches last keyword", function()
      assert.same({"qux"}, {parser.parse("1:qux")})
    end)

    it("fails on non-matching input", function()
      local result = parser.parse("1:xyz")
      assert.is_nil(result)
    end)

    it("fails on partial match", function()
      local result = parser.parse("1:fo")
      assert.is_nil(result)
    end)
  end)

  describe("backtracking on partial match (test2)", function()
    it("matches full trie alternatives", function()
      assert.same({"ab"}, {parser.parse("2:ab")})
      assert.same({"abc"}, {parser.parse("2:abc")})
      assert.same({"abd"}, {parser.parse("2:abd")})
    end)

    it("backtracks to fallback when trie fails partway", function()
      -- Input "2:ax": trie matches "a", then fails on "x"
      -- Must backtrack and match "a" via the fallback C(P"a")
      -- This test would have caught the original backtracking bug
      assert.same({"a"}, {parser.parse("2:ax")})
    end)

    it("backtracks on single char that doesn't continue", function()
      -- Input "2:a" - trie matches "a" but needs more chars for "ab"/"abc"/"abd"
      -- Should backtrack and match via fallback
      assert.same({"a"}, {parser.parse("2:a")})
    end)
  end)

  describe("overlapping prefixes (test3)", function()
    it("matches longest alternative", function()
      assert.same({"fork"}, {parser.parse("3:fork")})
    end)

    it("matches shorter alternatives", function()
      assert.same({"foo"}, {parser.parse("3:foo")})
      assert.same({"for"}, {parser.parse("3:for")})
    end)
  end)

  describe("trie in sequence (test4)", function()
    it("matches trie followed by set", function()
      assert.same({"xx", "a"}, {parser.parse("4:xxa")})
      assert.same({"xy", "b"}, {parser.parse("4:xyb")})
      assert.same({"xz", "c"}, {parser.parse("4:xzc")})
    end)
  end)

  describe("prefix fallback in sequence (test5)", function()
    it("does not accept non-existent prefix", function()
      local result = parser.parse("5:abcX")
      assert.is_nil(result)
    end)
  end)
end)

describe("trie optimization eligibility", function()
  it("applies trie optimization for 3+ literal choices", function()
    local P = pgen.P
    local grammar = {
      "test",
      test = P"aaa" + P"bbb" + P"ccc"
    }
    local code = pgen.compile(grammar, {parser_name = "test"})
    assert.truthy(code:match("Trie match"))
  end)

  it("does not apply trie for only 2 choices", function()
    local P = pgen.P
    local grammar = {
      "test",
      test = P"foo" + P"bar"
    }
    local code = pgen.compile(grammar, {parser_name = "test"})
    assert.falsy(code:match("Trie match"))
  end)

  it("does not apply trie when mixed with non-literals", function()
    local P, V = pgen.P, pgen.V
    local grammar = {
      "test",
      test = P"foo" + P"bar" + V"other",
      other = P"x"
    }
    local code = pgen.compile(grammar, {parser_name = "test"})
    assert.falsy(code:match("Trie match"))
  end)

  it("can be disabled with optimize=false", function()
    local P = pgen.P
    local grammar = {
      "test",
      test = P"aaa" + P"bbb" + P"ccc"
    }
    local code = pgen.compile(grammar, {parser_name = "test", optimize = false})
    assert.falsy(code:match("Trie match"))
  end)
end)

-- Unit tests for the full MoonScript parser (moonscript_pgen/).
-- Expected trees follow the reference parser's AST format exactly,
-- including [-1] position annotations. The differential suite in
-- moonscript_diff_spec.lua checks parity with the reference parser over
-- the whole .moon corpus; these are fast hand-checked anchors.

describe("moonscript_pgen", function()
  local parse

  setup(function()
    parse = require "moonscript_pgen.init"
  end)

  it("parses assignment", function()
    assert.same({
      {"assign",
        {{"ref", "x", [-1] = 1}},
        {{"number", "5", [-1] = 4}},
        [-1] = 1},
    }, parse.string("x = 5"))
  end)

  it("parses an open call", function()
    assert.same({
      {"chain",
        {"ref", "print", [-1] = 1},
        {"call", {{"ref", "x", [-1] = 6}}},
        [-1] = 1},
    }, parse.string("print x"))
  end)

  it("parses table with self-assign and string key", function()
    assert.same({
      {"assign",
        {{"ref", "t", [-1] = 1}},
        {{"table", {
          {{"key_literal", "name"}, {"ref", "name", [-1] = 11}},
          {{"string", '"', "k"}, {"ref", "v", [-1] = 17}},
        }, [-1] = 4}},
        [-1] = 1},
    }, parse.string([[t = {:name, "k": v}]]))
  end)

  it("parses a function literal with binary op body", function()
    assert.same({
      {"assign",
        {{"ref", "f", [-1] = 1}},
        {{"fndef", {{"a"}}, {}, "slim", {
          {"exp", {"ref", "a", [-1] = 11}, "+", {"number", "1", [-1] = 15}, [-1] = 11},
        }, [-1] = 4}},
        [-1] = 1},
    }, parse.string("f = (a) -> a + 1"))
  end)

  it("parses an indented block", function()
    assert.same({
      {"if", {"ref", "x", [-1] = 3}, {
        {"chain", {"ref", "y", [-1] = 8}, {"call", {}}, [-1] = 8},
      }, [-1] = 1},
    }, parse.string("if x\n  y!"))
  end)

  it("parses lua strings, reconstructing the open delimiter", function()
    assert.same({
      {"assign",
        {{"ref", "x", [-1] = 1}},
        {{"string", "[==[", "str", [-1] = 4}},
        [-1] = 1},
    }, parse.string("x = [==[str]==]"))
  end)

  it("parses an anonymous class with a nil name slot", function()
    local tree = assert(parse.string("x = class"))
    local class_node = tree[1][3][1]
    assert.same("class", class_node[1])
    assert.is_nil(class_node[2])
    assert.same("", class_node[3])
    assert.same({}, class_node[4])
  end)

  it("parses an empty file", function()
    assert.same({}, parse.string(""))
  end)

  it("parses a comment-only file", function()
    assert.same({}, parse.string("-- nothing here"))
  end)

  it("fails on unbalanced parens", function()
    local tree, err = parse.string("x = (a + b")
    assert.is_nil(tree)
    assert.is_string(err)
  end)

  it("fails on a bad outdent", function()
    assert.is_nil((parse.string("if x\n    y\n  z")))
  end)

  it("rejects a non-assignable left hand side", function()
    local tree, err = parse.string("f! = 5")
    assert.is_nil(tree)
    assert.matches("not assignable", err)
  end)

  it("keeps the parser reusable after a failed parse", function()
    assert.is_nil((parse.string("x = (")))
    assert.same({
      {"assign",
        {{"ref", "x", [-1] = 1}},
        {{"number", "1", [-1] = 4}},
        [-1] = 1},
    }, parse.string("x = 1"))
  end)

  it("disables do expressions inside loop headers", function()
    -- `do` after the while condition is the block keyword, not a do-expression
    local tree = assert(parse.string("while x do print 1"))
    assert.same("while", tree[1][1])
    -- but a do-expression is fine in normal expression position
    local tree2 = assert(parse.string("x = do\n  5"))
    assert.same("do", tree2[1][3][1][1])
  end)

  it("handles interpolation containing a lua string", function()
    local tree = assert(parse.string([=[x = "a#{ [[long]] }b"]=]))
    local str = tree[1][3][1]
    assert.same("string", str[1])
    assert.same("a", str[3])
    assert.same("interpolate", str[4][1])
    assert.same("string", str[4][2][1])
    assert.same("[[", str[4][2][2])
    assert.same("long", str[4][2][3])
    assert.same("b", str[5])
  end)
end)

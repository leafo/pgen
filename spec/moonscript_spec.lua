local pgen = require "pgen"

describe("moonscript parser", function()
  local parser

  before_each(function()
    parser = pgen.require("spec.parsers.moonscript")
  end)

  describe("basic values", function()
    it("parses numbers", function()
      assert.same({"block", {"number", "123"}}, parser.parse("123"))
    end)

    it("parses decimal numbers", function()
      assert.same({"block", {"number", "3.14"}}, parser.parse("3.14"))
    end)

    it("parses names", function()
      assert.same({"block", {"name", "foo"}}, parser.parse("foo"))
    end)

    it("parses names with underscores", function()
      assert.same({"block", {"name", "foo_bar"}}, parser.parse("foo_bar"))
    end)

    it("parses double-quoted strings", function()
      assert.same({"block", {"string", "hello"}}, parser.parse("\"hello\""))
    end)

    it("parses single-quoted strings", function()
      assert.same({"block", {"string", "world"}}, parser.parse("'world'"))
    end)
  end)

  describe("single-line statements", function()
    it("parses simple assignment", function()
      assert.same(
        {"block", {"assign", {"name", "x"}, {"number", "5"}}},
        parser.parse("x = 5")
      )
    end)

    it("parses assignment with string", function()
      assert.same(
        {"block", {"assign", {"name", "msg"}, {"string", "hello"}}},
        parser.parse("msg = \"hello\"")
      )
    end)
  end)

  describe("indented blocks", function()
    it("parses if with indented body", function()
      local result = parser.parse([[
if true
  x = 1]])
      assert.same({
        "block",
        {"if", {"name", "true"}, {"block", {"assign", {"name", "x"}, {"number", "1"}}}}
      }, result)
    end)

    it("parses if with multiple statements in body", function()
      local result = parser.parse([[
if true
  x = 1
  y = 2]])
      assert.same({
        "block",
        {"if", {"name", "true"}, {"block",
          {"assign", {"name", "x"}, {"number", "1"}},
          {"assign", {"name", "y"}, {"number", "2"}}
        }}
      }, result)
    end)

    it("ignores blank lines inside blocks", function()
      local result = parser.parse([[
if a

  x = 1
  
  y = 2]])
      assert.same({
        "block",
        {"if", {"name", "a"}, {"block",
          {"assign", {"name", "x"}, {"number", "1"}},
          {"assign", {"name", "y"}, {"number", "2"}}
        }}
      }, result)
    end)

    it("parses nested if statements", function()
      local result = parser.parse([[
if a
  if b
    x = 1]])
      assert.same({
        "block",
        {"if", {"name", "a"}, {"block",
          {"if", {"name", "b"}, {"block",
            {"assign", {"name", "x"}, {"number", "1"}}
          }}
        }}
      }, result)
    end)
  end)

  describe("multiple statements at same level", function()
    it("parses two assignments", function()
      local result = parser.parse([[
x = 1
y = 2]])
      assert.same({
        "block",
        {"assign", {"name", "x"}, {"number", "1"}},
        {"assign", {"name", "y"}, {"number", "2"}}
      }, result)
    end)

    it("parses statements after if block", function()
      local result = parser.parse([[
if a
  x = 1
y = 2]])
      assert.same({
        "block",
        {"if", {"name", "a"}, {"block",
          {"assign", {"name", "x"}, {"number", "1"}}
        }},
        {"assign", {"name", "y"}, {"number", "2"}}
      }, result)
    end)
  end)

  describe("edge cases", function()
    it("parses dedent across multiple levels", function()
      local result = parser.parse([[
if a
  if b
    x = 1
y = 2]])
      assert.same({
        "block",
        {"if", {"name", "a"}, {"block",
          {"if", {"name", "b"}, {"block",
            {"assign", {"name", "x"}, {"number", "1"}}
          }}
        }},
        {"assign", {"name", "y"}, {"number", "2"}}
      }, result)
    end)

    it("ignores leading blank lines", function()
      local result = parser.parse([[


x = 1]])
      assert.same({
        "block",
        {"assign", {"name", "x"}, {"number", "1"}}
      }, result)
    end)

    it("ignores blank lines between top-level statements", function()
      local result = parser.parse([[
x = 1

 	 

y = 2]])
      assert.same({
        "block",
        {"assign", {"name", "x"}, {"number", "1"}},
        {"assign", {"name", "y"}, {"number", "2"}}
      }, result)
    end)

    it("ignores trailing blank lines", function()
      local result = parser.parse([[
x = 1

]])
      assert.same({
        "block",
        {"assign", {"name", "x"}, {"number", "1"}}
      }, result)
    end)

    it("parses mixed tabs/spaces when widths match", function()
      local result = parser.parse([[
if a
	x = 1
  y = 2]])
      assert.same({
        "block",
        {"if", {"name", "a"}, {"block",
          {"assign", {"name", "x"}, {"number", "1"}},
          {"assign", {"name", "y"}, {"number", "2"}}
        }}
      }, result)
    end)

    it("fails when a block is not indented", function()
      local result = parser.parse([[
if a
x = 1]])
      assert.is_nil(result)
    end)

    it("fails when indent width does not match current level", function()
      local result = parser.parse([[
if a
  x = 1
 y = 2]])
      assert.is_nil(result)
    end)

    it("fails when a block header is followed only by blank lines", function()
      local result = parser.parse([[
if a
   
]])
      assert.is_nil(result)
    end)
  end)
end)

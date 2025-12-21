describe("lua_parser", function()
  local pgen = require "pgen"
  local lua_parser = pgen.require("spec.parsers.lua_parser")

  local function check_parse(input, success_expected)
    local result = lua_parser.parse(input)
    if success_expected then
      assert.is_not_nil(result, "Expected successful parse for: " .. input)
    else
      assert.is_nil(result, "Expected parse to fail for: " .. input)
    end
    return result
  end

  -- Helper to build common AST patterns
  local function ret(exp_content)
    return {"block", {"return", {"explist", {"exp", exp_content}}}}
  end

  describe("literals", function()
    it("parses nil", function()
      local result = check_parse("return nil", true)
      assert.same(ret({"nil"}), result)
    end)

    it("parses booleans", function()
      -- Note: Both true and false capture as just {"boolean"} - the actual value isn't distinguished
      local result = check_parse("return true", true)
      assert.same(ret({"boolean"}), result)

      result = check_parse("return false", true)
      assert.same(ret({"boolean"}), result)
    end)

    it("parses integers", function()
      local result = check_parse("return 42", true)
      assert.same(ret({"number", "42"}), result)

      result = check_parse("return 0", true)
      assert.same(ret({"number", "0"}), result)
    end)

    it("parses floats", function()
      local result = check_parse("return 3.14", true)
      assert.same(ret({"number", "3.14"}), result)

      result = check_parse("return .5", true)
      assert.same(ret({"number", ".5"}), result)

      result = check_parse("return 1.", true)
      assert.same(ret({"number", "1."}), result)
    end)

    it("parses hex numbers", function()
      local result = check_parse("return 0xFF", true)
      assert.same(ret({"number", "0xFF"}), result)

      result = check_parse("return 0x1A2B", true)
      assert.same(ret({"number", "0x1A2B"}), result)
    end)

    it("parses scientific notation", function()
      local result = check_parse("return 1e10", true)
      assert.same(ret({"number", "1e10"}), result)

      result = check_parse("return 1.5e-3", true)
      assert.same(ret({"number", "1.5e-3"}), result)
    end)

    it("parses double-quoted strings", function()
      local result = check_parse('return "hello"', true)
      assert.same(ret({"string", '"hello"'}), result)

      result = check_parse('return "hello world"', true)
      assert.same(ret({"string", '"hello world"'}), result)
    end)

    it("parses single-quoted strings", function()
      local result = check_parse("return 'hello'", true)
      assert.same(ret({"string", "'hello'"}), result)
    end)

    it("parses long strings", function()
      local result = check_parse("return [[hello]]", true)
      assert.same(ret({"string", "[[hello]]"}), result)

      result = check_parse("return [=[hello]=]", true)
      assert.same(ret({"string", "[=[hello]=]"}), result)

      result = check_parse("return [==[hello]==]", true)
      assert.same(ret({"string", "[==[hello]==]"}), result)
    end)

    it("rejects long strings with mismatched equals", function()
      check_parse("return [=[hello]]", false)    -- 1 open, 0 close
      check_parse("return [[hello]=]", false)    -- 0 open, 1 close
      check_parse("return [==[hello]=]", false)  -- 2 open, 1 close
    end)

    it("parses escape sequences in strings", function()
      local result = check_parse('return "line1\\nline2"', true)
      assert.same(ret({"string", '"line1\\nline2"'}), result)

      result = check_parse('return "tab\\there"', true)
      assert.same(ret({"string", '"tab\\there"'}), result)
    end)
  end)

  describe("expressions", function()
    it("parses simple arithmetic", function()
      local result = check_parse("return 1 + 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "+"}, {"number", "2"}
      }}}}, result)

      result = check_parse("return 3 - 4", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "3"}, {"binop", "-"}, {"number", "4"}
      }}}}, result)

      result = check_parse("return 5 * 6", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "5"}, {"binop", "*"}, {"number", "6"}
      }}}}, result)

      result = check_parse("return 7 / 8", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "7"}, {"binop", "/"}, {"number", "8"}
      }}}}, result)
    end)

    it("parses floor division", function()
      local result = check_parse("return 7 // 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "7"}, {"binop", "//"}, {"number", "2"}
      }}}}, result)
    end)

    it("parses modulo", function()
      local result = check_parse("return 10 % 3", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "10"}, {"binop", "%"}, {"number", "3"}
      }}}}, result)
    end)

    it("parses exponentiation", function()
      local result = check_parse("return 2 ^ 10", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "2"}, {"binop", "^"}, {"number", "10"}
      }}}}, result)
    end)

    it("parses comparison operators", function()
      local result = check_parse("return 1 < 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "<"}, {"number", "2"}
      }}}}, result)

      result = check_parse("return 1 > 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", ">"}, {"number", "2"}
      }}}}, result)

      result = check_parse("return 1 <= 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "<="}, {"number", "2"}
      }}}}, result)

      result = check_parse("return 1 >= 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", ">="}, {"number", "2"}
      }}}}, result)

      result = check_parse("return 1 == 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "=="}, {"number", "2"}
      }}}}, result)

      result = check_parse("return 1 ~= 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "~="}, {"number", "2"}
      }}}}, result)
    end)

    it("parses logical operators", function()
      local result = check_parse("return true and false", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"boolean"}, {"binop", "and"}, {"boolean"}
      }}}}, result)

      result = check_parse("return true or false", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"boolean"}, {"binop", "or"}, {"boolean"}
      }}}}, result)
    end)

    it("parses bitwise operators", function()
      local result = check_parse("return 1 & 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "&"}, {"number", "2"}
      }}}}, result)

      result = check_parse("return 1 | 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "|"}, {"number", "2"}
      }}}}, result)

      result = check_parse("return 1 ~ 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "~"}, {"number", "2"}
      }}}}, result)

      result = check_parse("return 1 << 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "<<"}, {"number", "2"}
      }}}}, result)

      result = check_parse("return 1 >> 2", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", ">>"}, {"number", "2"}
      }}}}, result)
    end)

    it("parses string concatenation", function()
      local result = check_parse('return "a" .. "b"', true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"string", '"a"'}, {"binop", ".."}, {"string", '"b"'}
      }}}}, result)
    end)

    it("parses unary operators", function()
      local result = check_parse("return -x", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"unop", "-", {"exp", {"prefixexp", {"name", "x"}}}}
      }}}}, result)

      result = check_parse("return not true", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"unop", "not", {"exp", {"boolean"}}}
      }}}}, result)

      result = check_parse("return #t", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"unop", "#", {"exp", {"prefixexp", {"name", "t"}}}}
      }}}}, result)

      result = check_parse("return ~n", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"unop", "~", {"exp", {"prefixexp", {"name", "n"}}}}
      }}}}, result)
    end)

    it("parses parenthesized expressions", function()
      local result = check_parse("return (1 + 2) * 3", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"prefixexp", {"paren", {"exp", {"number", "1"}, {"binop", "+"}, {"number", "2"}}}},
        {"binop", "*"},
        {"number", "3"}
      }}}}, result)
    end)

    it("parses chained binary operators", function()
      local result = check_parse("return 1 + 2 + 3", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "+"}, {"number", "2"}, {"binop", "+"}, {"number", "3"}
      }}}}, result)

      result = check_parse("return 1 + 2 * 3 - 4", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"number", "1"}, {"binop", "+"}, {"number", "2"}, {"binop", "*"}, {"number", "3"}, {"binop", "-"}, {"number", "4"}
      }}}}, result)
    end)

    it("parses vararg", function()
      local result = check_parse("return ...", true)
      assert.same(ret({"vararg"}), result)
    end)
  end)

  describe("tables", function()
    it("parses empty table", function()
      local result = check_parse("return {}", true)
      assert.same(ret({"table"}), result)
    end)

    it("parses array-style table", function()
      local result = check_parse("return {1, 2, 3}", true)
      assert.same(ret({"table", {"fields",
        {"exp_field", {"exp", {"number", "1"}}},
        {"exp_field", {"exp", {"number", "2"}}},
        {"exp_field", {"exp", {"number", "3"}}}
      }}), result)
    end)

    it("parses record-style table", function()
      local result = check_parse("return {a = 1, b = 2}", true)
      assert.same(ret({"table", {"fields",
        {"name_field", {"name", "a"}, {"exp", {"number", "1"}}},
        {"name_field", {"name", "b"}, {"exp", {"number", "2"}}}
      }}), result)
    end)

    it("parses bracketed keys", function()
      local result = check_parse('return {["key"] = "value"}', true)
      assert.same(ret({"table", {"fields",
        {"index_field", {"exp", {"string", '"key"'}}, {"exp", {"string", '"value"'}}}
      }}), result)
    end)

    it("parses mixed table", function()
      local result = check_parse("return {1, a = 2, [3] = 4}", true)
      assert.same(ret({"table", {"fields",
        {"exp_field", {"exp", {"number", "1"}}},
        {"name_field", {"name", "a"}, {"exp", {"number", "2"}}},
        {"index_field", {"exp", {"number", "3"}}, {"exp", {"number", "4"}}}
      }}), result)
    end)

    it("parses nested tables", function()
      local result = check_parse("return {{1, 2}, {3, 4}}", true)
      assert.same(ret({"table", {"fields",
        {"exp_field", {"exp", {"table", {"fields",
          {"exp_field", {"exp", {"number", "1"}}},
          {"exp_field", {"exp", {"number", "2"}}}
        }}}},
        {"exp_field", {"exp", {"table", {"fields",
          {"exp_field", {"exp", {"number", "3"}}},
          {"exp_field", {"exp", {"number", "4"}}}
        }}}}
      }}), result)
    end)

    it("parses trailing separator", function()
      local result = check_parse("return {1, 2, 3,}", true)
      assert.same(ret({"table", {"fields",
        {"exp_field", {"exp", {"number", "1"}}},
        {"exp_field", {"exp", {"number", "2"}}},
        {"exp_field", {"exp", {"number", "3"}}}
      }}), result)

      result = check_parse("return {a = 1;}", true)
      assert.same(ret({"table", {"fields",
        {"name_field", {"name", "a"}, {"exp", {"number", "1"}}}
      }}), result)
    end)
  end)

  describe("variables", function()
    it("parses simple variable", function()
      local result = check_parse("return x", true)
      assert.same(ret({"prefixexp", {"name", "x"}}), result)
    end)

    it("parses field access", function()
      local result = check_parse("return t.field", true)
      assert.same(ret({"prefixexp", {"name", "t"}, {"field", {"name", "field"}}}), result)
    end)

    it("parses index access", function()
      local result = check_parse("return t[1]", true)
      assert.same(ret({"prefixexp", {"name", "t"}, {"index", {"exp", {"number", "1"}}}}), result)

      result = check_parse('return t["key"]', true)
      assert.same(ret({"prefixexp", {"name", "t"}, {"index", {"exp", {"string", '"key"'}}}}), result)
    end)

    it("parses chained field access", function()
      local result = check_parse("return a.b.c", true)
      assert.same(ret({"prefixexp",
        {"name", "a"},
        {"field", {"name", "b"}},
        {"field", {"name", "c"}}
      }), result)
    end)

    it("parses chained index access", function()
      local result = check_parse("return a[1][2]", true)
      assert.same(ret({"prefixexp",
        {"name", "a"},
        {"index", {"exp", {"number", "1"}}},
        {"index", {"exp", {"number", "2"}}}
      }), result)
    end)

    it("parses mixed chained access", function()
      local result = check_parse("return a.b[1].c", true)
      assert.same(ret({"prefixexp",
        {"name", "a"},
        {"field", {"name", "b"}},
        {"index", {"exp", {"number", "1"}}},
        {"field", {"name", "c"}}
      }), result)
    end)
  end)

  describe("function calls", function()
    it("parses simple call", function()
      local result = check_parse("print()", true)
      assert.same({"block", {"call", {"name", "print"}, {"call"}}}, result)

      result = check_parse("print(1)", true)
      assert.same({"block", {"call", {"name", "print"}, {"call", {"explist", {"exp", {"number", "1"}}}}}}, result)

      result = check_parse("print(1, 2, 3)", true)
      assert.same({"block", {"call", {"name", "print"}, {"call", {"explist",
        {"exp", {"number", "1"}},
        {"exp", {"number", "2"}},
        {"exp", {"number", "3"}}
      }}}}, result)
    end)

    it("parses call with string argument", function()
      local result = check_parse('print "hello"', true)
      assert.same({"block", {"call", {"name", "print"}, {"call", {"string", '"hello"'}}}}, result)

      result = check_parse("print 'hello'", true)
      assert.same({"block", {"call", {"name", "print"}, {"call", {"string", "'hello'"}}}}, result)
    end)

    it("parses call with table argument", function()
      local result = check_parse("print {1, 2, 3}", true)
      assert.same({"block", {"call", {"name", "print"}, {"call", {"table", {"fields",
        {"exp_field", {"exp", {"number", "1"}}},
        {"exp_field", {"exp", {"number", "2"}}},
        {"exp_field", {"exp", {"number", "3"}}}
      }}}}}, result)
    end)

    it("parses method call", function()
      local result = check_parse("obj:method()", true)
      assert.same({"block", {"call", {"name", "obj"}, {"method", {"name", "method"}}}}, result)

      result = check_parse("obj:method(1, 2)", true)
      assert.same({"block", {"call", {"name", "obj"}, {"method", {"name", "method"}, {"explist",
        {"exp", {"number", "1"}},
        {"exp", {"number", "2"}}
      }}}}, result)
    end)

    it("parses chained calls", function()
      local result = check_parse("f()()", true)
      assert.same({"block", {"call", {"name", "f"}, {"call"}, {"call"}}}, result)
    end)

    it("parses call on field access", function()
      local result = check_parse("a.b()", true)
      assert.same({"block", {"call", {"name", "a"}, {"field", {"name", "b"}}, {"call"}}}, result)
    end)

    it("parses method call on expression", function()
      local result = check_parse("(getobj()):method()", true)
      assert.same({"block", {"call",
        {"paren", {"exp", {"prefixexp", {"name", "getobj"}, {"call"}}}},
        {"method", {"name", "method"}}
      }}, result)
    end)
  end)

  describe("statements", function()
    it("parses empty statement", function()
      local result = check_parse(";", true)
      assert.same({"block", {"empty"}}, result)
    end)

    it("parses assignment", function()
      local result = check_parse("x = 1", true)
      assert.same({"block", {"assign",
        {"varlist", {"var", {"name", "x"}}},
        {"explist", {"exp", {"number", "1"}}}
      }}, result)

      result = check_parse("x, y = 1, 2", true)
      assert.same({"block", {"assign",
        {"varlist", {"var", {"name", "x"}}, {"var", {"name", "y"}}},
        {"explist", {"exp", {"number", "1"}}, {"exp", {"number", "2"}}}
      }}, result)
    end)

    it("parses local declaration", function()
      local result = check_parse("local x", true)
      assert.same({"block", {"local", {"attnamelist", {"name", "x"}}}}, result)

      result = check_parse("local x = 1", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "x"}},
        {"explist", {"exp", {"number", "1"}}}
      }}, result)

      result = check_parse("local x, y = 1, 2", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "x"}, {"name", "y"}},
        {"explist", {"exp", {"number", "1"}}, {"exp", {"number", "2"}}}
      }}, result)
    end)

    it("parses local with attributes", function()
      local result = check_parse("local x <const> = 1", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "x"}, {"attrib", {"name", "const"}}},
        {"explist", {"exp", {"number", "1"}}}
      }}, result)

      result = check_parse("local x <close> = io.open('f')", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "x"}, {"attrib", {"name", "close"}}},
        {"explist", {"exp", {"prefixexp",
          {"name", "io"},
          {"field", {"name", "open"}},
          {"call", {"explist", {"exp", {"string", "'f'"}}}}
        }}}
      }}, result)
    end)

    it("parses do block", function()
      local result = check_parse("do end", true)
      assert.same({"block", {"do", {"block"}}}, result)

      result = check_parse("do local x = 1 end", true)
      assert.same({"block", {"do", {"block",
        {"local", {"attnamelist", {"name", "x"}}, {"explist", {"exp", {"number", "1"}}}}
      }}}, result)
    end)

    it("parses while loop", function()
      local result = check_parse("while true do end", true)
      assert.same({"block", {"while", {"exp", {"boolean"}}, {"block"}}}, result)

      result = check_parse("while x < 10 do x = x + 1 end", true)
      assert.same({"block", {"while",
        {"exp", {"prefixexp", {"name", "x"}}, {"binop", "<"}, {"number", "10"}},
        {"block", {"assign",
          {"varlist", {"var", {"name", "x"}}},
          {"explist", {"exp", {"prefixexp", {"name", "x"}}, {"binop", "+"}, {"number", "1"}}}
        }}
      }}, result)
    end)

    it("parses repeat loop", function()
      local result = check_parse("repeat until true", true)
      assert.same({"block", {"repeat", {"block"}, {"exp", {"boolean"}}}}, result)

      result = check_parse("repeat x = x + 1 until x > 10", true)
      assert.same({"block", {"repeat",
        {"block", {"assign",
          {"varlist", {"var", {"name", "x"}}},
          {"explist", {"exp", {"prefixexp", {"name", "x"}}, {"binop", "+"}, {"number", "1"}}}
        }},
        {"exp", {"prefixexp", {"name", "x"}}, {"binop", ">"}, {"number", "10"}}
      }}, result)
    end)

    it("parses numeric for loop", function()
      local result = check_parse("for i = 1, 10 do end", true)
      assert.same({"block", {"for_num",
        {"name", "i"},
        {"exp", {"number", "1"}},
        {"exp", {"number", "10"}},
        {"block"}
      }}, result)

      result = check_parse("for i = 1, 10, 2 do end", true)
      assert.same({"block", {"for_num",
        {"name", "i"},
        {"exp", {"number", "1"}},
        {"exp", {"number", "10"}},
        {"exp", {"number", "2"}},
        {"block"}
      }}, result)
    end)

    it("parses generic for loop", function()
      local result = check_parse("for k, v in pairs(t) do end", true)
      assert.same({"block", {"for_in",
        {"namelist", {"name", "k"}, {"name", "v"}},
        {"explist", {"exp", {"prefixexp",
          {"name", "pairs"},
          {"call", {"explist", {"exp", {"prefixexp", {"name", "t"}}}}}
        }}},
        {"block"}
      }}, result)

      result = check_parse("for i, v in ipairs(t) do end", true)
      assert.same({"block", {"for_in",
        {"namelist", {"name", "i"}, {"name", "v"}},
        {"explist", {"exp", {"prefixexp",
          {"name", "ipairs"},
          {"call", {"explist", {"exp", {"prefixexp", {"name", "t"}}}}}
        }}},
        {"block"}
      }}, result)
    end)

    it("parses if statement", function()
      local result = check_parse("if true then end", true)
      assert.same({"block", {"if", {"exp", {"boolean"}}, {"block"}}}, result)

      result = check_parse("if true then x = 1 end", true)
      assert.same({"block", {"if",
        {"exp", {"boolean"}},
        {"block", {"assign",
          {"varlist", {"var", {"name", "x"}}},
          {"explist", {"exp", {"number", "1"}}}
        }}
      }}, result)
    end)

    it("parses if-else statement", function()
      local result = check_parse("if true then x = 1 else x = 2 end", true)
      assert.same({"block", {"if",
        {"exp", {"boolean"}},
        {"block", {"assign", {"varlist", {"var", {"name", "x"}}}, {"explist", {"exp", {"number", "1"}}}}},
        {"block", {"assign", {"varlist", {"var", {"name", "x"}}}, {"explist", {"exp", {"number", "2"}}}}}
      }}, result)
    end)

    it("parses if-elseif-else statement", function()
      local result = check_parse("if a then x = 1 elseif b then x = 2 else x = 3 end", true)
      assert.same({"block", {"if",
        {"exp", {"prefixexp", {"name", "a"}}},
        {"block", {"assign", {"varlist", {"var", {"name", "x"}}}, {"explist", {"exp", {"number", "1"}}}}},
        {"exp", {"prefixexp", {"name", "b"}}},
        {"block", {"assign", {"varlist", {"var", {"name", "x"}}}, {"explist", {"exp", {"number", "2"}}}}},
        {"block", {"assign", {"varlist", {"var", {"name", "x"}}}, {"explist", {"exp", {"number", "3"}}}}}
      }}, result)
    end)

    it("parses break", function()
      local result = check_parse("while true do break end", true)
      assert.same({"block", {"while",
        {"exp", {"boolean"}},
        {"block", {"break"}}
      }}, result)
    end)

    it("parses goto and labels", function()
      local result = check_parse("goto label", true)
      assert.same({"block", {"goto", {"name", "label"}}}, result)

      result = check_parse("::label::", true)
      assert.same({"block", {"label", {"name", "label"}}}, result)

      result = check_parse("::label:: goto label", true)
      assert.same({"block",
        {"label", {"name", "label"}},
        {"goto", {"name", "label"}}
      }, result)
    end)

    it("parses return statement", function()
      local result = check_parse("return", true)
      assert.same({"block", {"return"}}, result)

      result = check_parse("return 1", true)
      assert.same({"block", {"return", {"explist", {"exp", {"number", "1"}}}}}, result)

      result = check_parse("return 1, 2, 3", true)
      assert.same({"block", {"return", {"explist",
        {"exp", {"number", "1"}},
        {"exp", {"number", "2"}},
        {"exp", {"number", "3"}}
      }}}, result)
    end)
  end)

  describe("function definitions", function()
    it("parses global function", function()
      local result = check_parse("function f() end", true)
      assert.same({"block", {"function",
        {"funcname", {"name", "f"}},
        {"funcbody", {"block"}}
      }}, result)

      result = check_parse("function f(a, b) end", true)
      assert.same({"block", {"function",
        {"funcname", {"name", "f"}},
        {"funcbody", {"params", {"namelist", {"name", "a"}, {"name", "b"}}}, {"block"}}
      }}, result)
    end)

    it("parses local function", function()
      local result = check_parse("local function f() end", true)
      assert.same({"block", {"local_function",
        {"name", "f"},
        {"funcbody", {"block"}}
      }}, result)
    end)

    it("parses anonymous function", function()
      local result = check_parse("return function() end", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"function", {"funcbody", {"block"}}}
      }}}}, result)

      result = check_parse("return function(a, b) return a + b end", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"function", {"funcbody",
          {"params", {"namelist", {"name", "a"}, {"name", "b"}}},
          {"block", {"return", {"explist", {"exp",
            {"prefixexp", {"name", "a"}},
            {"binop", "+"},
            {"prefixexp", {"name", "b"}}
          }}}}
        }}
      }}}}, result)
    end)

    it("parses vararg function", function()
      local result = check_parse("function f(...) end", true)
      assert.same({"block", {"function",
        {"funcname", {"name", "f"}},
        {"funcbody", {"params", {"vararg"}}, {"block"}}
      }}, result)

      result = check_parse("function f(a, ...) end", true)
      assert.same({"block", {"function",
        {"funcname", {"name", "f"}},
        {"funcbody", {"params", {"namelist", {"name", "a"}}}, {"block"}}
      }}, result)
    end)

    it("parses method definition", function()
      local result = check_parse("function t:method() end", true)
      assert.same({"block", {"function",
        {"funcname", {"name", "t"}, {"name", "method"}},
        {"funcbody", {"block"}}
      }}, result)
    end)

    it("parses nested function name", function()
      local result = check_parse("function a.b.c() end", true)
      assert.same({"block", {"function",
        {"funcname", {"name", "a"}, {"name", "b"}, {"name", "c"}},
        {"funcbody", {"block"}}
      }}, result)
    end)
  end)

  describe("comments", function()
    -- Comments are consumed as whitespace and don't appear in the AST
    it("parses single-line comment", function()
      local result = check_parse("-- comment\nreturn 1", true)
      assert.same({"block", {"return", {"explist", {"exp", {"number", "1"}}}}}, result)

      result = check_parse("return 1 -- comment", true)
      assert.same({"block", {"return", {"explist", {"exp", {"number", "1"}}}}}, result)
    end)

    it("parses multi-line comment", function()
      local result = check_parse("--[[ comment ]]\nreturn 1", true)
      assert.same({"block", {"return", {"explist", {"exp", {"number", "1"}}}}}, result)

      result = check_parse("--[[ multi\nline\ncomment ]]\nreturn 1", true)
      assert.same({"block", {"return", {"explist", {"exp", {"number", "1"}}}}}, result)
    end)

    it("parses long comment with equals", function()
      check_parse("--[=[ comment ]=]\nreturn 1", true)
      check_parse("--[=[ multi\nline ]=]\nreturn 1", true)
      check_parse("--[==[ comment ]==]\nreturn 1", true)
    end)

    it("rejects long comments with mismatched equals", function()
      check_parse("--[=[ comment ]]\nreturn 1", false)   -- 1 open, 0 close
      check_parse("--[[ comment ]=]\nreturn 1", false)   -- 0 open, 1 close
      check_parse("--[==[ comment ]=]\nreturn 1", false) -- 2 open, 1 close
    end)
  end)

  describe("complex programs", function()
    it("parses multiple statements", function()
      local result = check_parse([[
        local x = 1
        local y = 2
        return x + y
      ]], true)
      assert.same({"block",
        {"local", {"attnamelist", {"name", "x"}}, {"explist", {"exp", {"number", "1"}}}},
        {"local", {"attnamelist", {"name", "y"}}, {"explist", {"exp", {"number", "2"}}}},
        {"return", {"explist", {"exp",
          {"prefixexp", {"name", "x"}},
          {"binop", "+"},
          {"prefixexp", {"name", "y"}}
        }}}
      }, result)
    end)

    it("parses nested control structures", function()
      local result = check_parse([[
        for i = 1, 10 do
          if i % 2 == 0 then
            print(i)
          end
        end
      ]], true)
      assert.same({"block", {"for_num",
        {"name", "i"},
        {"exp", {"number", "1"}},
        {"exp", {"number", "10"}},
        {"block", {"if",
          {"exp",
            {"prefixexp", {"name", "i"}},
            {"binop", "%"},
            {"number", "2"},
            {"binop", "=="},
            {"number", "0"}
          },
          {"block", {"call",
            {"name", "print"},
            {"call", {"explist", {"exp", {"prefixexp", {"name", "i"}}}}}
          }}
        }}
      }}, result)
    end)

    it("parses function with body", function()
      local result = check_parse([[
        function factorial(n)
          if n <= 1 then
            return 1
          else
            return n * factorial(n - 1)
          end
        end
      ]], true)
      assert.same({"block", {"function",
        {"funcname", {"name", "factorial"}},
        {"funcbody",
          {"params", {"namelist", {"name", "n"}}},
          {"block", {"if",
            {"exp", {"prefixexp", {"name", "n"}}, {"binop", "<="}, {"number", "1"}},
            {"block", {"return", {"explist", {"exp", {"number", "1"}}}}},
            {"block", {"return", {"explist", {"exp",
              {"prefixexp", {"name", "n"}},
              {"binop", "*"},
              {"prefixexp",
                {"name", "factorial"},
                {"call", {"explist", {"exp",
                  {"prefixexp", {"name", "n"}},
                  {"binop", "-"},
                  {"number", "1"}
                }}}}
            }}}}
          }}
        }
      }}, result)
    end)
  end)

  describe("error cases", function()
    it("rejects keywords as identifiers", function()
      check_parse("local and = 1", false)
      check_parse("local if = 1", false)
      check_parse("local function = 1", false)
    end)

    it("rejects unclosed blocks", function()
      check_parse("if true then", false)
      check_parse("while true do", false)
      check_parse("function f()", false)
    end)

    it("rejects invalid syntax", function()
      check_parse("return +", false)
      check_parse("local", false)
    end)
  end)
end)

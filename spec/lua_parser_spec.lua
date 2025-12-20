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

  describe("literals", function()
    it("parses nil", function()
      local result = check_parse("return nil", true)
      assert.is_not_nil(result)
    end)

    it("parses booleans", function()
      check_parse("return true", true)
      check_parse("return false", true)
    end)

    it("parses integers", function()
      check_parse("return 42", true)
      check_parse("return 0", true)
    end)

    it("parses floats", function()
      check_parse("return 3.14", true)
      check_parse("return .5", true)
      check_parse("return 1.", true)
    end)

    it("parses hex numbers", function()
      check_parse("return 0xFF", true)
      check_parse("return 0x1A2B", true)
    end)

    it("parses scientific notation", function()
      check_parse("return 1e10", true)
      check_parse("return 1.5e-3", true)
    end)

    it("parses double-quoted strings", function()
      check_parse('return "hello"', true)
      check_parse('return "hello world"', true)
    end)

    it("parses single-quoted strings", function()
      check_parse("return 'hello'", true)
    end)

    it("parses long strings", function()
      check_parse("return [[hello]]", true)
      check_parse("return [=[hello]=]", true)
      check_parse("return [==[hello]==]", true)
    end)

    pending("rejects long strings with mismatched equals", function()
      check_parse("return [=[hello]]", false)    -- 1 open, 0 close
      check_parse("return [[hello]=]", false)    -- 0 open, 1 close
      check_parse("return [==[hello]=]", false)  -- 2 open, 1 close
    end)

    it("parses escape sequences in strings", function()
      check_parse('return "line1\\nline2"', true)
      check_parse('return "tab\\there"', true)
    end)
  end)

  describe("expressions", function()
    it("parses simple arithmetic", function()
      check_parse("return 1 + 2", true)
      check_parse("return 3 - 4", true)
      check_parse("return 5 * 6", true)
      check_parse("return 7 / 8", true)
    end)

    it("parses floor division", function()
      check_parse("return 7 // 2", true)
    end)

    it("parses modulo", function()
      check_parse("return 10 % 3", true)
    end)

    it("parses exponentiation", function()
      check_parse("return 2 ^ 10", true)
    end)

    it("parses comparison operators", function()
      check_parse("return 1 < 2", true)
      check_parse("return 1 > 2", true)
      check_parse("return 1 <= 2", true)
      check_parse("return 1 >= 2", true)
      check_parse("return 1 == 2", true)
      check_parse("return 1 ~= 2", true)
    end)

    it("parses logical operators", function()
      check_parse("return true and false", true)
      check_parse("return true or false", true)
    end)

    it("parses bitwise operators", function()
      check_parse("return 1 & 2", true)
      check_parse("return 1 | 2", true)
      check_parse("return 1 ~ 2", true)
      check_parse("return 1 << 2", true)
      check_parse("return 1 >> 2", true)
    end)

    it("parses string concatenation", function()
      check_parse('return "a" .. "b"', true)
    end)

    it("parses unary operators", function()
      check_parse("return -x", true)
      check_parse("return not true", true)
      check_parse("return #t", true)
      check_parse("return ~n", true)
    end)

    it("parses parenthesized expressions", function()
      check_parse("return (1 + 2) * 3", true)
    end)

    it("parses chained binary operators", function()
      check_parse("return 1 + 2 + 3", true)
      check_parse("return 1 + 2 * 3 - 4", true)
    end)

    it("parses vararg", function()
      check_parse("return ...", true)
    end)
  end)

  describe("tables", function()
    it("parses empty table", function()
      check_parse("return {}", true)
    end)

    it("parses array-style table", function()
      check_parse("return {1, 2, 3}", true)
    end)

    it("parses record-style table", function()
      check_parse("return {a = 1, b = 2}", true)
    end)

    it("parses bracketed keys", function()
      check_parse('return {["key"] = "value"}', true)
    end)

    it("parses mixed table", function()
      check_parse("return {1, a = 2, [3] = 4}", true)
    end)

    it("parses nested tables", function()
      check_parse("return {{1, 2}, {3, 4}}", true)
    end)

    it("parses trailing separator", function()
      check_parse("return {1, 2, 3,}", true)
      check_parse("return {a = 1;}", true)
    end)
  end)

  describe("variables", function()
    it("parses simple variable", function()
      check_parse("return x", true)
    end)

    it("parses field access", function()
      check_parse("return t.field", true)
    end)

    it("parses index access", function()
      check_parse("return t[1]", true)
      check_parse('return t["key"]', true)
    end)

    it("parses chained field access", function()
      check_parse("return a.b.c", true)
    end)

    it("parses chained index access", function()
      check_parse("return a[1][2]", true)
    end)

    it("parses mixed chained access", function()
      check_parse("return a.b[1].c", true)
    end)
  end)

  describe("function calls", function()
    it("parses simple call", function()
      check_parse("print()", true)
      check_parse("print(1)", true)
      check_parse("print(1, 2, 3)", true)
    end)

    it("parses call with string argument", function()
      check_parse('print "hello"', true)
      check_parse("print 'hello'", true)
    end)

    it("parses call with table argument", function()
      check_parse("print {1, 2, 3}", true)
    end)

    it("parses method call", function()
      check_parse("obj:method()", true)
      check_parse("obj:method(1, 2)", true)
    end)

    it("parses chained calls", function()
      check_parse("f()()", true)
    end)

    it("parses call on field access", function()
      check_parse("a.b()", true)
    end)

    it("parses method call on expression", function()
      check_parse("(getobj()):method()", true)
    end)
  end)

  describe("statements", function()
    it("parses empty statement", function()
      check_parse(";", true)
    end)

    it("parses assignment", function()
      check_parse("x = 1", true)
      check_parse("x, y = 1, 2", true)
    end)

    it("parses local declaration", function()
      check_parse("local x", true)
      check_parse("local x = 1", true)
      check_parse("local x, y = 1, 2", true)
    end)

    it("parses local with attributes", function()
      check_parse("local x <const> = 1", true)
      check_parse("local x <close> = io.open('f')", true)
    end)

    it("parses do block", function()
      check_parse("do end", true)
      check_parse("do local x = 1 end", true)
    end)

    it("parses while loop", function()
      check_parse("while true do end", true)
      check_parse("while x < 10 do x = x + 1 end", true)
    end)

    it("parses repeat loop", function()
      check_parse("repeat until true", true)
      check_parse("repeat x = x + 1 until x > 10", true)
    end)

    it("parses numeric for loop", function()
      check_parse("for i = 1, 10 do end", true)
      check_parse("for i = 1, 10, 2 do end", true)
    end)

    it("parses generic for loop", function()
      check_parse("for k, v in pairs(t) do end", true)
      check_parse("for i, v in ipairs(t) do end", true)
    end)

    it("parses if statement", function()
      check_parse("if true then end", true)
      check_parse("if true then x = 1 end", true)
    end)

    it("parses if-else statement", function()
      check_parse("if true then x = 1 else x = 2 end", true)
    end)

    it("parses if-elseif-else statement", function()
      check_parse("if a then x = 1 elseif b then x = 2 else x = 3 end", true)
    end)

    it("parses break", function()
      check_parse("while true do break end", true)
    end)

    it("parses goto and labels", function()
      check_parse("goto label", true)
      check_parse("::label::", true)
      check_parse("::label:: goto label", true)
    end)

    it("parses return statement", function()
      check_parse("return", true)
      check_parse("return 1", true)
      check_parse("return 1, 2, 3", true)
    end)
  end)

  describe("function definitions", function()
    it("parses global function", function()
      check_parse("function f() end", true)
      check_parse("function f(a, b) end", true)
    end)

    it("parses local function", function()
      check_parse("local function f() end", true)
    end)

    it("parses anonymous function", function()
      check_parse("return function() end", true)
      check_parse("return function(a, b) return a + b end", true)
    end)

    it("parses vararg function", function()
      check_parse("function f(...) end", true)
      check_parse("function f(a, ...) end", true)
    end)

    it("parses method definition", function()
      check_parse("function t:method() end", true)
    end)

    it("parses nested function name", function()
      check_parse("function a.b.c() end", true)
    end)
  end)

  describe("comments", function()
    it("parses single-line comment", function()
      check_parse("-- comment\nreturn 1", true)
      check_parse("return 1 -- comment", true)
    end)

    it("parses multi-line comment", function()
      check_parse("--[[ comment ]]\nreturn 1", true)
      check_parse("--[[ multi\nline\ncomment ]]\nreturn 1", true)
    end)

    pending("parses long comment with equals", function()
      check_parse("--[=[ comment ]=]\nreturn 1", true)
      check_parse("--[=[ multi\nline ]=]\nreturn 1", true)
      check_parse("--[==[ comment ]==]\nreturn 1", true)
    end)

    pending("rejects long comments with mismatched equals", function()
      check_parse("--[=[ comment ]]\nreturn 1", false)   -- 1 open, 0 close
      check_parse("--[[ comment ]=]\nreturn 1", false)   -- 0 open, 1 close
      check_parse("--[==[ comment ]=]\nreturn 1", false) -- 2 open, 1 close
    end)
  end)

  describe("complex programs", function()
    it("parses multiple statements", function()
      check_parse([[
        local x = 1
        local y = 2
        return x + y
      ]], true)
    end)

    it("parses nested control structures", function()
      check_parse([[
        for i = 1, 10 do
          if i % 2 == 0 then
            print(i)
          end
        end
      ]], true)
    end)

    it("parses function with body", function()
      check_parse([[
        function factorial(n)
          if n <= 1 then
            return 1
          else
            return n * factorial(n - 1)
          end
        end
      ]], true)
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

describe("pgen.errors", function()
  local errors = require("pgen.errors")

  describe("format", function()
    it("formats error in middle of single line", function()
      local msg = errors.format("hello world", 7, "test_error")
      assert.equal([[test_error at line 1, column 7:
  hello world
        ^]], msg)
    end)

    it("formats error at position 1", function()
      local msg = errors.format("hello", 1, "at_start")
      assert.equal([[at_start at line 1, column 1:
  hello
  ^]], msg)
    end)

    it("formats error at end of line", function()
      local msg = errors.format("hello", 6, "at_end")
      assert.equal([[at_end at line 1, column 6:
  hello
       ^]], msg)
    end)

    it("formats error on first line of multi-line input", function()
      local input = "line one\nline two\nline three"
      local msg = errors.format(input, 5, "first_line")
      assert.equal([[first_line at line 1, column 5:
  line one
      ^]], msg)
    end)

    it("formats error on second line of multi-line input", function()
      local input = "line one\nline two\nline three"
      -- Position 14 is 'w' in "two" (9 chars for "line one\n" + 5 for "line ")
      local msg = errors.format(input, 14, "second_line")
      assert.equal([[second_line at line 2, column 5:
  line two
      ^]], msg)
    end)

    it("formats error on third line of multi-line input", function()
      local input = "line one\nline two\nline three"
      -- Position 19 is start of "line three" (9 + 9 = 18, so 19 is 'l')
      local msg = errors.format(input, 19, "third_line")
      assert.equal([[third_line at line 3, column 1:
  line three
  ^]], msg)
    end)

    it("uses 'error' as default label when none provided", function()
      local msg = errors.format("test", 3, nil)
      assert.equal([[error at line 1, column 3:
  test
    ^]], msg)
    end)

    it("handles empty line", function()
      local input = "first\n\nthird"
      -- Position 7 is the empty line (after first \n)
      local msg = errors.format(input, 7, "empty_line")
      assert.equal("empty_line at line 2, column 1:\n  \n  ^", msg)
    end)

    it("handles error right after newline", function()
      local input = "first\nsecond"
      -- Position 7 is 's' in "second"
      local msg = errors.format(input, 7, "after_newline")
      assert.equal([[after_newline at line 2, column 1:
  second
  ^]], msg)
    end)

    it("supports color option", function()
      local msg = errors.format("test", 3, "my_error", {color = true})
      -- Check that ANSI escape codes are present (ESC = \27)
      assert.truthy(msg:match("\27%["))
      -- Check that content is still there
      assert.truthy(msg:match("my_error"))
      assert.truthy(msg:match("line 1"))
      assert.truthy(msg:match("column 3"))
      assert.truthy(msg:match("test"))
      assert.truthy(msg:match("%^"))
    end)

    it("shows context lines with context option", function()
      local input = "line one\nline two\nline three\nline four\nline five"
      -- Error on line 3, column 6 ('t' in 'three')
      -- Position: "line one\nline two\n" = 18 chars, then "line " = 5, so pos 24
      local msg = errors.format(input, 24, "test_error", {context = 1})
      assert.equal([[test_error at line 3, column 6:
2 | line two
3 | line three
         ^
4 | line four]], msg)
    end)

    it("shows context lines at start of file", function()
      local input = "line one\nline two\nline three"
      -- Error on line 1, column 5
      local msg = errors.format(input, 5, "test_error", {context = 2})
      assert.equal([[test_error at line 1, column 5:
1 | line one
        ^
2 | line two
3 | line three]], msg)
    end)

    it("shows context lines at end of file", function()
      local input = "line one\nline two\nline three"
      -- Error on line 3, column 6 ('t' in 'three')
      -- Position: 9 + 9 + 6 = 24
      local msg = errors.format(input, 24, "test_error", {context = 2})
      assert.equal([[test_error at line 3, column 6:
1 | line one
2 | line two
3 | line three
         ^]], msg)
    end)

    it("pads line numbers for alignment", function()
      -- Create input with 10+ lines to test padding
      local lines = {}
      for i = 1, 12 do
        lines[i] = "line " .. i
      end
      local input = table.concat(lines, "\n")
      -- Error on line 10, column 1
      -- Position: sum of "line X\n" for lines 1-9
      local pos = 0
      for i = 1, 9 do
        pos = pos + #("line " .. i) + 1  -- +1 for newline
      end
      pos = pos + 1  -- column 1 of line 10
      local msg = errors.format(input, pos, "test_error", {context = 1})
      assert.equal([[test_error at line 10, column 1:
 9 | line 9
10 | line 10
     ^
11 | line 11]], msg)
    end)

    it("context = 0 behaves like no context option", function()
      local input = "line one\nline two\nline three"
      -- Error on line 2, column 5
      local msg_no_context = errors.format(input, 14, "test_error")
      local msg_zero_context = errors.format(input, 14, "test_error", {context = 0})
      assert.equal(msg_no_context, msg_zero_context)
    end)
  end)
end)

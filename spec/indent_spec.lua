local pgen = require "pgen"

describe("indenter", function()
  local parser

  setup(function()
    parser = pgen.require("spec.parsers.indent")
  end)

  describe("1 - block language (check/advance/pop)", function()
    it("parses a single line", function()
      assert.same({"a"}, parser.parse("1:a"))
    end)

    it("parses siblings at the same level", function()
      assert.same({"a", "b"}, parser.parse("1:a\nb"))
    end)

    it("parses a nested block", function()
      assert.same({"a", {"b"}, "c"}, parser.parse("1:a:\n  b\nc"))
    end)

    it("parses deeply nested blocks with multi-level dedent", function()
      assert.same(
        {"a", {"b", {"c"}}, "d"},
        parser.parse("1:a:\n  b:\n      c\nd")
      )
    end)

    it("allows a tab-indented block with space-indented sibling of same width", function()
      -- tab = 4, so "\t" and "    " are the same level
      assert.same({"a", {"b", "c"}}, parser.parse("1:a:\n\tb\n    c"))
    end)

    it("fails when a block is not indented", function()
      assert.is_nil(parser.parse("1:a:\nb"))
    end)

    it("fails when sibling indent does not match", function()
      assert.is_nil(parser.parse("1:a:\n  b\n c"))
    end)

    it("does not leak state between parse calls", function()
      assert.is_nil(parser.parse("1:a:\nb"))
      assert.same({"a", {"b"}}, parser.parse("1:a:\n  b"))
      assert.same({"a", {"b"}}, parser.parse("1:a:\n  b"))
    end)
  end)

  describe("2 - transactional rollback across failed alternative", function()
    it("rolls back a push from a failed alternative", function()
      -- first alternative advances the indent stack then fails; the
      -- fallback requires the stack top to still be 0
      assert.same({"clean"}, {parser.parse("2:a\n  b")})
    end)
  end)

  describe("3/4 - prevent", function()
    it("blocks advance after prevent", function()
      assert.same({"blocked"}, {parser.parse("3:a\n  x")})
    end)

    it("allows advance without prevent", function()
      assert.same({"advanced"}, {parser.parse("4:a\n  x")})
    end)
  end)

  describe("5 - unconditional push", function()
    it("pushes measured width and checks it on the next line", function()
      assert.same({"ok"}, {parser.parse("5:{\n   x\n   y")})
    end)

    it("fails when the next line width differs", function()
      assert.is_nil(parser.parse("5:{\n   x\n  y"))
    end)
  end)

  describe("6 - constant ops (flag stack)", function()
    it("cpush/ctop/pop drive a do-stack style flag", function()
      assert.same(
        {"allowed", "disabled", "allowed"},
        {parser.parse("6:")}
      )
    end)
  end)

  describe("7 - pop semantics", function()
    it("pops the seed element, fails on empty, and rolls back pops", function()
      -- first alternative: pop, pop -> second pop fails on empty stack
      -- second alternative: pop, ctop on empty stack fails
      -- third alternative only works if the previous pops were rolled back
      assert.same({"single"}, {parser.parse("7:")})
    end)
  end)

  describe("8 - lookahead purity", function()
    it("rolls back a push performed inside a lookahead", function()
      assert.same({"pure"}, {parser.parse("8:")})
    end)
  end)

  describe("9 - Cmt rollback", function()
    it("rolls back inner stack ops when the callback rejects the match", function()
      assert.same({"cmt-clean"}, {parser.parse("9:")})
    end)
  end)

  describe("10/11 - tab width configuration", function()
    it("measures a tab as 4 by default", function()
      assert.same({"tab4"}, {parser.parse("10:a\n\tb")})
    end)

    it("measures a tab as 2 when configured", function()
      assert.same({"tab2"}, {parser.parse("11:a\n\tb")})
    end)

    it("fails the width check when tab width does not line up", function()
      -- two spaces measure 2, but the default indenter expects top == 4
      -- after advancing over a tab... here advance pushes 2 and ctop eq 4 fails
      assert.is_nil(parser.parse("10:a\n  b"))
    end)
  end)
end)

# Test Grammar Guidelines

This library compiles a grammar object into .c code. In order to test it
end-to-end it must be compiled with GCC and loaded as a shared library. The
`pgen.require()` function streamlines all of this.

First create a grammar in `spec/parsers/` as a plain Lua module, eg.
`my_grammar.lua`. Then you can load it in the test environment using
`pgen.require`.


```lua
local pgen = require "pgen"
local parser = pgen.require("spec.parsers.my_grammar")
local result = parser.parse("input string")
```

**How it works internally:**

1. Loads the grammar Lua file via `require()`
2. Compiles the grammar table to C code via `pgen.compile()`
3. Invokes GCC internally to compile C to a shared object (.so)
4. Loads the .so via `package.loadlib()`
5. Cleans up temporary files
6. Returns the ready-to-use parser module

## Structuring Test Grammars

A grammar can only have one entry point (the start rule). To test multiple rules or scenarios through a single grammar, use a **prefix selector pattern**:

```lua
local pgen = require "pgen"
local P, V, C = pgen.P, pgen.V, pgen.C

return {
  "test",  -- start rule name

  -- Main dispatcher: prefix selects which test to run
  test = P"1:" * V"test1" +
         P"2:" * V"test2" +
         P"3:" * V"test3",

  -- Individual test rules
  test1 = C(P"foo" + P"bar"),
  test2 = C(P"hello") * C(P"world"),
  test3 = C(P"abc")^1,
}
```

## Writing Specs

In the spec file, use the prefix to target specific rules:

```lua
describe("my feature", function()
  local pgen = require "pgen"
  local parser

  before_each(function()
    parser = pgen.require("spec.parsers.my_grammar")
  end)

  describe("test1 - basic matching", function()
    it("matches foo", function()
      assert.same({"foo"}, {parser.parse("1:foo")})
    end)

    it("fails on invalid input", function()
      assert.is_nil(parser.parse("1:xyz"))
    end)
  end)

  describe("test2 - sequences", function()
    it("captures both parts", function()
      assert.same({"hello", "world"}, {parser.parse("2:helloworld")})
    end)
  end)
end)
```

## Key Points

- Place grammar files in `spec/parsers/`
- Name them descriptively: `feature_name.lua`
- Use numbered prefixes (`"1:"`, `"2:"`, etc.) to select test scenarios
- Each `testN` rule tests a specific aspect of the feature
- Add comments explaining what each test rule exercises
- `parser.parse()` returns multiple values for multiple captures; wrap in `{}` to collect them

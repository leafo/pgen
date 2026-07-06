parser generator in Lua that takes LPeg-like pattern definitions written in Lua
and generates a Lua module written in C that can parse input strings.

This is just an experiment that will eventually find its way into [moonparse](https://github.com/leafo/moonparse)

## Features

- LPeg-inspired syntax for defining grammars
- Generates parser in pure C
- Supports common pattern types: literals, character ranges, character sets
- Operators for sequences, choices, repetition and optional patterns
- Supports captures, table captures, constant captures, named captures, and match-time captures
- Indentation-sensitive parsing via match-time stacks that roll back on backtracking

## Usage

```lua
local pgen = require "pgen"

-- Import pattern constructors
local P, R, S, V = pgen.P, pgen.R, pgen.S, pgen.V

-- Define grammar
local grammar = {
  "numbers", -- initial rule name

  numbers = (V"ws" * V"number")^1 * -1,

  ws = (S" \t\n\r")^0,
  number = P"-"^-1 * (V"float" + V"integer"),
  integer = R("09")^1,
  float = V"integer" * P"." * V"integer"
}

-- Print out generated C code
print(pgen.compile(grammar, {
  parser_name = "my_parser"
}))
```

See [Compilation](#compilation) for more details on how to compile the generated C code.

## Pattern Types

- `P(string)` - Match literal string
- `P(number)` - Match exactly n characters (positive number) or fail if n characters can be matched (negative number)
- `R(...)` - Match character ranges (can handle multiple ranges: `R("az", "AZ", "09")`)
- `S(set)` - Match character in set
- `V(rule)` - Reference another rule by name in a grammar
- `C(patt)` - Capture text matched by patt
- `Ct(patt)` - Capture table, any captures created by patt are wrapped into a single table
- `Cp()` - Capture current position without consuming input
- `Cc(...)` - Constant capture, consumes no input and always matches (appends the given values as captures)
- `L(patt)` - Lookahead pattern (matches without consuming input)
- `Cg(patt, name)` - Named capture group (creates named field in parent `Ct`)
- `Cmt(patt, code)` - Match-time capture (evaluates Lua code during matching)

### Extensions and Differences

Patterns specific to this library that aren't in LPeg:

- `Cn(patt, n)` - Numbered capture (select the nth capture from inner pattern, use `n=0` to discard all captures)
- `Cmb(name)` - Match backreference (matches the same text captured by `Cg` with the given name)

Unlike LPeg's `Cmt` which takes a function, pgen's `Cmt(patt, code)` takes a **string of Lua code**. This code is embedded into the generated C parser and executed via the Lua C API during parsing. The code receives `(subject, pos, ...)` where `...` are any captures from the inner pattern, and should return a position (to advance), `true` (to succeed), or `false`/`nil` (to fail).

## Indentation-Sensitive Parsing

PEGs can't express indentation-based block structure (Python, MoonScript,
YAML, ...) with lookahead alone, since matching a block requires comparing
line indentation against surrounding context. pgen provides **indenters** for
this: integer stacks that live inside the generated parser and are
manipulated by patterns at match time.

```lua
local ind = pgen.indenter{
  tab_width = 4, -- width a "\t" counts for (space = 1), default 4
  initial = 0,   -- value the stack is seeded with, default 0
}
```

Each call to `pgen.indenter()` declares one independent stack in the compiled
parser. The stack is reset at the start of every `parse()` call. The returned
object provides patterns that operate on the stack; none of them produce
captures.

Indent operations measure the run of space/tab characters at the current
position and compare its width against the top of the stack:

- `ind.check` - Consume the whitespace if its width equals the top of the stack, otherwise fail
- `ind.advance` - Push the width if it is greater than the top of the stack, otherwise fail. Consumes nothing (the whitespace is left for a following `check`)
- `ind.push` - Unconditionally push the measured width and consume the whitespace
- `ind.prevent` - Push a sentinel value that causes any nested `advance` to fail. Consumes nothing
- `ind.pop` - Pop the stack, failing if it is empty

Constant operations ignore the input entirely, useful for tracking
match-time flags (e.g. MoonScript's `do` disambiguation):

- `ind.cpush(n)` - Push the constant integer `n`
- `ind.ctop(cmp, n)` - Succeed if `top cmp n` holds, where `cmp` is one of `"eq"`, `"ne"`, `"lt"`, `"le"`, `"gt"`, `"ge"`. Fails on an empty stack

A minimal block-structured grammar:

```lua
local pgen = require "pgen"
local P, R, S, V, C, Ct = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Ct

local ind = pgen.indenter{}

return {
  "File",

  File = V"Block" * -P(1),

  -- lines at the same indentation level
  Block = Ct(V"Line" * (P"\n" * V"Line")^0),

  -- every line must sit at the current level exactly
  Line = ind.check * V"Statement",

  -- "name:" opens a nested block at any deeper indentation
  Statement = C(R"az"^1) * (P":" * P"\n" * ind.advance * V"Block" * ind.pop)^-1,
}
```

```
a:
  b:
      c
d
```

parses into `{"a", {"b", {"c"}}, "d"}`.

### Transactional Semantics

All indenter operations are **transactional**: every push and pop is recorded
on an internal trail, and when the parser backtracks past an operation it is
automatically undone. A failed alternative in a choice, a failed iteration of
a repeat, or a rejected `Cmt` always leaves the stack exactly as it found it,
so grammars never need cleanup patterns to keep the stack balanced across
failure paths.

Lookahead (`L`) and negative predicates (`-patt`) are state-pure: stack
operations performed inside them are rolled back even when the predicate
succeeds. This means a stack operation cannot communicate state out of a
lookahead — use `advance` (which internally measures ahead without consuming)
rather than wrapping `push` in `L()`.

## Labeled Failures

Use `T(label)` to throw a labeled failure with a descriptive error name. Unlike regular failures, labeled failures propagate through choice (`+`) and repeat (`^`) operators, allowing you to signal unrecoverable errors.

- `T(label)` - Throw a labeled failure with the given label string

When a labeled failure occurs, `parse()` returns three values: `nil, label, position`

Example grammar with error labels:

```lua
local pgen = require "pgen"
local P, R, V, T, C = pgen.P, pgen.R, pgen.V, pgen.T, pgen.C

return {
  "json",
  json = V"value" * (P(-1) + T"expected_eof"),
  value = V"string" + V"number" + T"expected_value",
  string = P'"' * C((P(1) - P'"')^0) * (P'"' + T"expected_closing_quote"),
  number = C(P"-"^-1 * R"09"^1)
}
```

## Error Formatting

The `pgen.errors` module formats labeled failures from `T()` into human-readable messages. It requires the `pos` (position) value returned by a labeled failure.

```lua
local errors = require "pgen.errors"

local result, label, pos = parser.parse(input)
if not result then
  if pos then
    -- Labeled failure from T()
    print(errors.format(input, pos, label))
  else
    -- Regular parse failure (no position available)
    print("Parse error")
  end
end
```

Output:

```
expected_closing_quote at line 3, column 15:
  {"name": "test
                ^
```

### Options

- `color` - Use ANSI colors for terminal output
- `context` - Number of lines to show above and below the error line

```lua
errors.format(input, pos, label, {color = true, context = 2})
```

Output with context:

```
expected_colon at line 5, column 8:
3 |   "name": "test",
4 |   "items": [
5 |     {"key" "value"}
             ^
6 |   ]
7 | }
```

## Operators

- `a * b` - Sequence: match a followed by b
- `a + b` - Choice: match a or b
- `-a` - Negative predicate, continue only if a can't be matched
- `a^n` - Matches at least n repetitions of pattern
- `a^-n` - Matches at most n repetitions of pattern
- `a - b` - Difference: match a only if b doesn't match at current position (implemented as `-b * a`)
- `#a` - Lookahead: matches a without consuming input (shorthand for `L(a)`)
- `a / n` - Numbered capture: shorthand for `Cn(a, n)`

## Compilation

To compile the generated C code as a Lua module, follow these steps:

1. Ensure you have a C compiler installed (e.g., GCC).
2. Use the following command to compile the `.c` file into a shared library, adjusting for your Lua version if necessary:

   ```bash
   gcc -shared -o <module_name>.so -fPIC <c_file_name>.c `pkg-config --cflags --libs lua5.1`
   ```

   - Replace `<module_name>` with the desired name of your Lua module.
   - Replace `<c_file_name>` with the name of the generated C file.

3. Place the resulting shared library (`.so` file) in a directory included in your Lua `package.cpath`.

4. In your Lua script, load the module using `require`:

   ```lua
   local my_parser = require "<module_name>"
   local result = my_parser.parse("your input string")
   ```

For development and testing, you can use `pgen.require(module_name, opts)` to
dynamically compile and load grammars. This will `os.execute` to GCC to compile
the grammar to a shared library in `/tmp/` and then load it immediately with
`package.loadlib`. In production environments it recommended to compile to C
ahead of time and build shared modules with your build system to avoid
expensive start-up time.


```lua
-- path/to/grammar.lua

local pgen = require "pgen"
local P = pgen.P

-- Define grammar and return it
return {
  "strings", -- initial rule name

  strings = P"hello" + P"world"
}
```


```lua
local pgen = require "pgen"
local parser = pgen.require("path.to.grammar") -- uses same search path as require()
local result = parser.parse("hello")
```


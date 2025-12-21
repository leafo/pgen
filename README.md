parser generator in Lua that takes LPeg-like pattern definitions written in Lua
and generates a Lua module written in C that can parse input strings.

This is just an experiment that will eventually find its way into [moonparse](https://github.com/leafo/moonparse)

## Features

- LPeg-inspired syntax for defining grammars
- Generates parser in pure C
- Supports common pattern types: literals, character ranges, character sets
- Operators for sequences, choices, repetition and optional patterns
- Supports captures, table captures, constant captures, named captures, and match-time captures

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


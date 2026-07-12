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

**Lua 5.1 compatibility note:** pgen patterns are plain Lua tables, and Lua 5.1's `__len` metamethod only works on userdata, not tables. This means the `#` operator for lookahead doesn't work in Lua 5.1. Use `L(patt)` explicitly instead of `#patt`.

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
  initial = 0,   -- value the stack starts with, default 0
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

Use `T(label)` to throw a labeled failure with a descriptive error name. Unlike regular failures, labeled failures propagate through choice (`+`) and repeat (`^`) operators, allowing you to signal unrecoverable errors. Labels thrown inside predicates (`L(patt)`, `-patt`) are swallowed and treated as ordinary failures, so speculative parses cannot leak hard errors.

- `T(label)` - Throw a labeled failure with the given label string

When a labeled failure occurs, `parse()` returns three values: `nil, label, position`

`T()` branches only execute after the preceding alternatives have already
failed, so labels add no work to successful parses.

**Caution with backtracking grammars:** a label turns a failure that would
normally backtrack into an error for the entire parse. If an enclosing
choice relies on that failure to fall through to another alternative — for
example when one alternative tries to parse text as code that a later
alternative would consume as string content — a label can reject input
that has a valid parse. Only throw where no other alternative could ever
succeed, and verify against a test corpus.

## Failure Positions

Every generated parser tracks the **furthest failure position**: the deepest
input position where a match attempt failed. Since the parser can only
attempt a position after successfully matching everything before it, this is
the point of deepest progress — usually right at the actual error.

On an ordinary (unlabeled) failure, `parse()` returns `nil, message, position`
where `message` is `nil` unless the parser was compiled with `--pgen-errors`,
and `position` is the 1-indexed furthest failure position.

The position is recorded in the failure paths of multi-character literals,
tries, predicates, `Cmb`/`Cmt`, and indenter operations. Single-character
matchers are skipped: they fail far more often than anything else, and any
position they fail at is also tried by larger patterns, so skipping them
costs no precision. The overhead is not measurable; compile with
`-DPGEN_NO_FURTHEST` to remove it entirely.

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
  -- label is the T() label, or nil for a regular failure; pos is the
  -- throw position or the furthest failure position respectively
  print(errors.format(input, pos, label or "parse error"))
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

## Parser Limits

Generated parsers guard against pathological input and grammars:

- **Recursion depth**: parsing deeply nested input recurses on the C stack.
  Past a configurable limit (default 5000) the parser raises a Lua error
  (catch with `pcall`) instead of overflowing the C stack. Configure with the
  `max_depth` option to `pgen.compile`/`pgen.require`, or override at C
  compile time with `-DPGEN_MAX_DEPTH=n`.
- **Capture count**: captures are built on the Lua stack, so the number of
  simultaneously pending captures is bounded by the Lua build's
  `LUAI_MAXCSTACK` (8000 in stock Lua 5.1). Exceeding it raises a clean Lua
  error.
- **Empty loops**: `pgen.compile` rejects unbounded repetitions (`patt^n` for
  `n >= 0`) whose body can match the empty string, since such a loop would
  never advance. This mirrors LPeg's "loop body may accept empty string"
  error. The check is conservative for recursive rule references: a loop body
  is rejected unless it provably consumes input.

## Optimizations

The optimizer will transform the grammar before code generation to produce more
efficient parsers. Optimizations can be disabled with the `--no-optimize` CLI
flag or `optimize = false` option.

### Trie Optimization

When a choice contains 3 or more string literals, they are combined into a trie
(prefix tree) data structure. This allows the parser to match keywords and
operators more efficiently by sharing common prefixes. To take advantage of
this optimization you must meet the requirements below.

```lua
-- Before optimization: linear search through alternatives
local keywords = P"function" + P"for" + P"if" + P"in" + P"local" + P"return"

-- After optimization: single trie lookup with shared prefixes
-- "f" -> "o" -> "r" (matches "for")
--              -> "unction" (matches "function")
-- "i" -> "f" (matches "if")
--     -> "n" (matches "in")
-- etc.
```

**Requirements for trie eligibility:**
- At least 3 alternatives
- All alternatives must be string literals (`P"..."`)
- No empty strings
- Longer strings must appear before their prefixes (e.g., `P"function" + P"fun"` not `P"fun" + P"function"`)

### Capture Table Optimization

The optimizer analyzes `Ct()` capture tables to determine if they contain any named captures (`Cg(patt, name)`). When a `Ct` only contains positional captures, the generated code can use a more efficient array-based approach instead of checking for named fields.

```lua
-- Marked as array-only (no Cg inside)
Ct(C(R"az"^1) * (P"," * C(R"az"^1))^0)

-- Not optimized (contains named capture)
Ct(Cg(C(R"az"^1), "first") * P"," * C(R"az"^1))
```

This optimization follows references through `V()` rules to ensure correctness even when captures are defined in other rules.

### Backtrack State Analysis

Sequences, repetitions (`patt^n`), lookahead (`#patt`), and negation
(`-patt`) all need to save parser state so they can backtrack. Before
generating that code, the generator checks whether the enclosed pattern can
actually change any state besides the input position, meaning capture log
entries or indenter stack operations. If it can't, the generated code only
saves and restores the input position instead of taking a full snapshot
(input position, capture log length, Lua stack top, and indenter stack undo
trail).

```lua
-- Fast path: the loop body is capture-free, so a failed iteration only
-- restores the input position
(R"az"^1 * P",")^0

-- Full snapshot: a failed iteration must also unwind pending captures
(C(R"az"^1) * P",")^0
```

The analysis resolves `V()` rule references, including recursive and mutually
recursive rules. It errs on the side of caution: match-time captures (`Cmt`),
indenter operations, and unrecognized pattern types always get the full
snapshot.

Note that unlike the transforms above, this happens during code generation
and is always applied, even with `--no-optimize`.

**Writing grammars that benefit:**

- **Keep predicates capture-free.** Captures inside `#patt` or `-patt` are
  discarded even when the pattern succeeds, so a capture there never produces
  a value but still forces the full snapshot. Match with a capture-free
  predicate, then capture in the pattern that actually consumes the input.

- **Capture where the match is committed.** A single capture anywhere in a
  sequence puts the whole sequence on the full-snapshot path. In a choice
  between alternatives that frequently fail and backtrack, discriminate first
  with capture-free patterns and save the captures for the part of the
  grammar that runs once the alternative is decided.


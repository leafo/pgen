# CLAUDE.md

> Last updated at commit: `c48c4881ce56e050cda9515ab3c44ac04e7d078d`

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Core Commands

### Building and Testing

```bash
# Build and run tests using busted (test framework)
make busted
```

## Project Architecture

PGen is a Lua parser generator that takes LPEG-like pattern definitions and generates Lua modules in C for parsing strings.

### Directory Structure

```
pgen/
├── pgen.lua              # Core API module
├── pgen_cli.lua          # CLI tool
├── Makefile              # Build automation
├── pgen/                 # Core implementation modules
│   ├── generator.lua     # C code generation
│   ├── optimize.lua      # Grammar optimizations (trie, flattening)
│   ├── types.lua         # Type constants (P=1, R=2, etc.)
│   └── visitor.lua       # AST visitor pattern for traversal/transformation
├── examples/             # Example grammars (calc, JSON, numbers)
├── moonscript_pgen/      # Full MoonScript parser built on pgen
│   ├── grammar.lua       # Grammar (port of moonscript/moonscript/parse.lua)
│   ├── tree.lua          # Transform helpers for the grammar's Cfn callbacks
│   └── init.lua          # parse.string() API (compiles parser on first use)
└── spec/                 # Test suite (busted framework)
    ├── *_spec.lua        # Test files
    └── parsers/          # Test grammars and their generated C/so files
```

The MoonScript parser is validated differentially: `spec/moonscript_diff_spec.lua`
asserts tree-identical output (including `[-1]` positions) against the
reference LPeg parser over every `.moon` file in the `moonscript/` checkout.
It is skipped automatically when `moonscript/` or lpeg is unavailable. Design
notes and porting decisions live in **moonscript_pgen/moonscript_indent.md**.

### Key Components

1. **pgen.lua**: Core module with pattern constructors and compilation functions
2. **pgen/generator.lua**: C code generator (~1400 lines) that transforms grammar into parser code
3. **pgen/optimize.lua**: Grammar optimizer (trie optimization for literal alternatives, choice flattening)
4. **pgen/visitor.lua**: Generic visitor pattern for AST traversal and transformation
5. **pgen/types.lua**: Type constants for pattern types
6. **pgen_cli.lua**: Command-line interface for the generator

### Workflow

1. Define a grammar using pattern constructors (P, R, S, V, etc.)
2. Compile the grammar to C code with `pgen.compile()`
3. Compile the C code to a shared object (.so file)
4. Load the shared object as a Lua module
5. Use the module's `parse()` function to parse input strings

For development/testing, use `pgen.require()` to dynamically compile and load grammars:

```lua
local pgen = require("pgen")
local parser = pgen.require("grammar.module") --> uses same search path as require()
parser.parse("input string")
```

### Pattern Types

- `P(string)` - Match literal string
- `P(n)` - Match exactly n characters (or fail if n < 0 and that many chars remain)
- `R(start, end)` - Match character range
- `S(set)` - Match character in set
- `V(rule)` - Reference another rule
- `C(patt)` - Capture matched text
- `Ct(patt)` - Capture into table (collects all inner captures)
- `Cp()` - Capture current position without consuming input
- `Cc(value)` - Capture constant value
- `L(patt)` - Lookahead (match without consuming input)
- `Cg(patt, name)` - Capture group (creates named field in parent Ct)
- `Cn(patt, n)` - Numbered capture (select nth capture from inner pattern)
- `Cmb(name)` - Match backreference: matches the same text captured by `Cg(patt, name)`
- `Cmt(patt, code)` - Match-time capture: evaluates Lua code during matching. Code receives `(subject, pos, ...)` where `...` are captures from inner pattern. Return position to advance, `true` to succeed, `false`/`nil` to fail, or position + values for extra captures

If you are adding a new type, please ensure that **pgen/visitor.lua** knows how
to traverse it. Additionally, it might be worth scanning **pgen/optimize.lua**
to ensure the new type doesn't interfere with any optimizers.

### Indenters (indentation-sensitive parsing)

`pgen.indenter(opts)` declares a match-time integer stack that lives in the
generated parser, for indent-based grammars (see **moonscript_pgen/moonscript_indent.md**).
Options: `tab_width` (default 4), `initial` (default 0). All operations are
**transactional**: pushes/pops are recorded on an undo trail and reversed when
the parser backtracks past them, so failed alternatives never leak stack
state. Lookahead (`L`) and negation are state-pure. None of the ops produce
captures.

```lua
local ind = pgen.indenter{tab_width = 4}
```

Indent ops (measure the run of space/tab at the current position; space = 1,
tab = tab_width):

- `ind.check` - consume the whitespace, succeed if width == top of stack
- `ind.advance` - consume nothing, push width if width > top (else fail)
- `ind.push` - consume the whitespace, always push measured width
- `ind.prevent` - consume nothing, push a sentinel so any nested advance fails
- `ind.pop` - pop the stack (fails if empty)

Constant ops (consume nothing; useful for do-stack style flags):

- `ind.cpush(n)` - push constant n
- `ind.ctop(cmp, n)` - succeed if `top cmp n`; cmp is one of `"eq"`, `"ne"`,
  `"lt"`, `"le"`, `"gt"`, `"ge"` (fails on an empty stack)

Each `pgen.indenter()` call declares one independent stack; the generator
assigns stack ids at compile time. Typical block structure:

```lua
Line = ind.check * V"Statement"
InBlock = ind.advance * V"Block" * ind.pop
```

### Failure Reporting

- Ordinary parse failure: `parse()` returns `nil, message, position` where
  `message` is nil unless compiled with `--pgen-errors`, and `position` is
  the 1-indexed **furthest failure position** (deepest input position where
  a match attempt failed — tracked in multi-char literals, tries,
  predicates, Cmb/Cmt, and indenter ops; disable with `-DPGEN_NO_FURTHEST`).
- Labeled failure from `T(label)`: returns `nil, label, position`. Labels
  propagate through choice/repeat but are swallowed inside predicates.
- `pgen.errors.format(input, pos, label)` renders line/column messages.

### Parser Limits and Safety

- **Recursion depth**: rule calls recurse on the C stack; the generated parser
  aborts with a Lua error (catch with `pcall`) past `PGEN_MAX_DEPTH` (default
  5000). Configure per grammar with the `max_depth` option to
  `pgen.compile`/`pgen.require`, or override at C compile time with
  `-DPGEN_MAX_DEPTH=n`.
- **Capture count**: captures are recorded in a C-side log during matching
  and only materialized into Lua values after the parse succeeds, so
  captures inside tables are unbounded. Only the number of top-level return
  values is bounded by the Lua build's `LUAI_MAXCSTACK` (8000 in stock Lua
  5.1); exceeding it raises a clean Lua error rather than undefined
  behavior.
- **Nullable loops**: `pgen.compile` rejects unbounded repetitions (`patt^n`
  for n >= 0) whose body can match the empty string ("loop body may accept
  empty string"), since they would hang the parser. The analysis lives in
  **pgen/analyze.lua** and is conservative for recursive rule references.

### Using the Visitor Module

The `pgen.visitor` module provides utilities for traversing and transforming grammar ASTs:

```lua
local visitor = require("pgen.visitor")

-- Visit all nodes in a grammar, optionally replacing them
local new_grammar = visitor.visit_grammar(grammar, function(node, replace)
  -- Inspect node.type, node.value, etc.
  -- Call replace(new_node) to substitute this node
  if node.type == types.P and node.value == "old" then
    replace(visitor.copy_node(node, {value = "new"}))
  end
end)

-- Visit a single pattern tree
local new_pattern = visitor.visit_pattern(pattern, visitor_fn)

-- Copy a node with optional property overrides
local new_node = visitor.copy_node(node, {value = "modified"})
```

The visitor uses copy-on-write semantics: when a node is replaced, all ancestor nodes are copied to preserve immutability of the original grammar. The original grammar/pattern is never mutated.

### Operators

- `a * b` - Sequence: match a followed by b
- `a + b` - Choice: match a or b
- `-a` - Negative predicate: succeed only if a doesn't match (consumes nothing)
- `#a` - Lookahead: same as L(a)
- `a^n` - Match at least n repetitions of a
- `a^-n` - Match at most n repetitions of a
- `a / n` - Numbered capture: same as Cn(a, n). Use `/0` to discard captures

### CLI Usage

```bash
lua pgen_cli.lua grammar.lua [options]
```

Options:
- `-o file.c` - Output to C file
- `-c file.so` - Compile directly to shared object
- `-j` - Output grammar as JSON (useful for debugging optimizations)
- `-n name` - Set parser name (affects C function names)
- `--pgen-errors` - Enable detailed error messages in generated parser
- `--no-optimize` - Disable grammar optimizations

### Code Generation Process

1. Pattern definitions are converted to an abstract syntax tree
2. The optimizer transforms the AST (flattens choices, builds tries for literals)
3. The AST is translated to C code with corresponding parsing functions
4. The C code includes a Lua module interface with `parse()` function
5. The generated C code is compiled into a shared object
6. The shared object is loaded as a Lua module

### Testing

Tests use the busted framework. Test grammars live in `spec/parsers/`.

The compiled `.so` and `.c` files in `spec/parsers/` are not actually used by
the test suite, they are built by the make command so that we can track changes
to compiled output via version control. The tests will always compile on the
fly when using `pgen.require()`

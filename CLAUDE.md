# CLAUDE.md

> Last updated at commit: `086feaf02c32539679fccd5e403362cc5ad11bb5`

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
└── spec/                 # Test suite (busted framework)
    ├── *_spec.lua        # Test files
    └── parsers/          # Test grammars and their generated C/so files
```

### Key Components

1. **pgen.lua**: Core module with pattern constructors and compilation functions
2. **pgen/generator.lua**: C code generator (~800 lines) that transforms grammar into parser code
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

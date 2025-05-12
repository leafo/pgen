# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Core Commands

### Building and Testing

```bash
# Build and run tests using busted (test framework)
make busted
```

## Project Architecture

PGen is a Lua parser generator that takes LPEG-like pattern definitions and generates Lua modules in C for parsing strings.

### Key Components

1. **pgen.lua**: Core module with pattern constructors (P, R, S, V) and compilation functions
2. **pgen/generator.lua**: C code generator that transforms grammar into parser code
3. **pgen_cli.lua**: Command-line interface for the generator
4. **examples/**: Example grammar files (calculator, JSON, numbers)
5. **spec/**: Test specifications using the busted framework

### Workflow

1. Define a grammar using pattern constructors (P, R, S, V)
2. Compile the grammar to C code with pgen.compile()
3. Compile the C code to a shared object (.so file)
4. Load the shared object as a Lua module
5. Use the module's parse() function to parse input strings

### Pattern Types

- `P(string)` - Match literal string
- `R(start, end)` - Match character range
- `S(set)` - Match character in set
- `V(rule)` - Reference another rule
- `C(patt)` - Capture pattern
- `Ct(patt)` - Capture table
- `Cp()` - Capture current position without consuming input

### Operators

- `a * b` - Sequence: match a followed by b
- `a + b` - Choice: match a or b
- `-a` - Negative predicate, only match if a doesn't match
- `a^n` - Match at least n repetitions of a
- `a^-n` - Match at most n repetitions of a

### Code Generation Process

1. Pattern definitions are converted to an abstract syntax tree
2. The AST is translated to C code with corresponding parsing functions
3. The C code includes a Lua module interface
4. The generated C code is compiled into a shared object
5. The shared object is loaded as a Lua module


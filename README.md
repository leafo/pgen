# PGen: Lua Pattern Generator for C

PGen is a parser generator in Lua that takes LPEG-like pattern definitions and generates corresponding C code parsers.

## Features

- LPEG-inspired syntax for defining grammars
- Generates standalone C parsers
- Supports common pattern types: literals, character ranges, character sets
- Operators for sequences, choices, repetition and optional patterns

## Usage

```lua
local pgen = require "pgen"

-- Import pattern constructors
local P, R, S, V = pgen.P, pgen.R, pgen.S, pgen.V

-- Define grammar
local grammar = {
  -- rule name = pattern definition
  main = P"hello" + P" " + P"world",
  
  -- Use rule references with V
  alt = V"main" * P"!" + P"?"
}

-- Compile to C
pgen.compile(grammar, {
  output_file = "my_parser.c",
  parser_name = "my_parser"
})
```

## Pattern Types

- `P(string)` - Match literal string
- `R(start, end)` - Match character range
- `S(set)` - Match character in set
- `V(rule)` - Reference another rule

## Operators

- `a + b` - Sequence: match a followed by b
- `a * b` - Choice: match a or b
- `-a` - Optional: match a or nothing
- `a^0` - Match a zero or more times
- `a^n` - Match a exactly n times

## Compilation

The generated C code includes:

- A parser struct with state tracking
- Functions for each grammar rule
- Error tracking and reporting
- Helper functions to initialize and run the parser

## Building

Compile the generated C file with your favorite C compiler:

```bash
gcc -o my_parser my_parser.c
```


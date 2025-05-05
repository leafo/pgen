#!/usr/bin/env lua5.1

local pgen = require "pgen"

local function print_usage()
  print("PGen: Lua Pattern Generator for C")
  print("Usage: pgen_cli.lua [options] input_file")
  print("")
  print("Options:")
  print("  -o, --output FILE    Output C file (default: parser.c)")
  print("  -n, --name NAME      Parser name (default: parser)")
  print("  -h, --help           Show this help message")
  print("")
  print("Example:")
  print("  pgen_cli.lua -o my_parser.c -n my_parser grammar.lua")
end

local args = {...}
local input_file
local output_file = "parser.c"
local parser_name = "parser"

local i = 1
while i <= #args do
  local arg = args[i]
  if arg == "-h" or arg == "--help" then
    print_usage()
    os.exit(0)
  elseif arg == "-o" or arg == "--output" then
    i = i + 1
    output_file = args[i]
  elseif arg == "-n" or arg == "--name" then
    i = i + 1
    parser_name = args[i]
  elseif not input_file then
    input_file = arg
  else
    print("Error: Unexpected argument: " .. arg)
    print_usage()
    os.exit(1)
  end
  i = i + 1
end

if not input_file then
  print("Error: No input file specified")
  print_usage()
  os.exit(1)
end

-- Load the grammar file
local chunk, err = loadfile(input_file)
if not chunk then
  print("Error loading input file: " .. (err or "unknown error"))
  os.exit(1)
end

-- Execute the grammar file to get the grammar
local success, result = pcall(chunk)
if not success then
  print("Error executing input file: " .. (result or "unknown error"))
  os.exit(1)
end

-- Check if the result is a table (grammar)
if type(result) ~= "table" then
  print("Error: Input file must return a grammar table")
  os.exit(1)
end

-- Compile the grammar
local output, err = pgen.compile(result, {
  output_file = output_file,
  parser_name = parser_name
})

if not output then
  print("Error generating parser: " .. (err or "unknown error"))
  os.exit(1)
end

print("Parser generated: " .. output)

#!/usr/bin/env lua5.1

local pgen = require "pgen"
local argparse = require "argparse"
local os = require "os"

local parser = argparse()
  :name("pgen")
  :description("PGen: Lua Pattern Generator for C")
  :epilog("Example: pgen_cli.lua -o my_parser.c -n my_parser grammar.lua -s my_parser.so")

parser:argument("input_file", "Input grammar file")
  :args(1)

parser:option("-o --output", "Output C file")
  :argname("FILE")

parser:option("-s --shared", "Output shared object file")
  :argname("FILE")

parser:option("-n --name", "Parser name")
  :argname("NAME")
  :default("parser")

parser:flag("--pgen-errors", "Generate error messages on failed parse paths (warning: substantial performance penalty)")
  :default(false)

local args = parser:parse()

local input_file = args.input_file
local output_file = args.output

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
  parser_name = args.name,
  pgen_errors = args.pgen_errors
})

if not output then
  print("Error generating parser: " .. (err or "unknown error"))
  os.exit(1)
end

-- If shared file option is provided, shell out to gcc to create shared object
if args.shared then
  local gcc_command = string.format("gcc -shared -o %s -O3 -fPIC -x c - `pkg-config --cflags --libs lua5.1`", args.shared)

  io.stderr:write("Running command: " .. gcc_command .. "\n")

  local gcc_process = io.popen(gcc_command, "w")
  gcc_process:write(output)
  gcc_process:close()
  io.stderr:write("Success\n")
-- Write output to a C file
elseif output_file then
  local file = io.open(output_file, "w")
  if not file then
    print("Error: Could not open output file: " .. output_file)
    os.exit(1)
  end

  file:write(output)
  file:close()
  io.stderr:write("Success\n")
-- print the output
else
  print(output)
end


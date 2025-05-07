#!/usr/bin/env lua5.1

local pgen = require "pgen"
local argparse = require "argparse"

local parser = argparse()
  :name("pgen")
  :description("PGen: Lua Pattern Generator for C")
  :epilog("Example: pgen_cli.lua -o my_parser.c -n my_parser grammar.lua")

parser:argument("input_file", "Input grammar file")
  :args(1)

parser:option("-o --output", "Output C file")
  :argname("FILE")
  :default("parser.c")

parser:option("-n --name", "Parser name")
  :argname("NAME")
  :default("parser")

local args = parser:parse()

local input_file = args.input_file
local output_file = args.output
local parser_name = args.name

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

print("Success")

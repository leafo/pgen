#!/usr/bin/env lua

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

parser:flag("--no-optimize", "Disable grammar optimization passes")
  :default(false)

parser:flag("--json", "Output grammar as JSON instead of generating C code")
  :default(false)

parser:option("--vendor-errors", "Copy the pgen.errors module source to FILE, for projects that ship generated parsers without a runtime pgen dependency")
  :argname("FILE")

local args = parser:parse()

if args.vendor_errors then
  local errors = require "pgen.errors"
  local source = debug.getinfo(errors.format, "S").source
  if source:sub(1, 1) ~= "@" then
    print("Error: could not locate pgen.errors source file")
    os.exit(1)
  end

  local f, err = io.open(source:sub(2), "r")
  if not f then
    print("Error reading pgen.errors source: " .. err)
    os.exit(1)
  end
  local contents = f:read("*a")
  f:close()

  local out, out_err = io.open(args.vendor_errors, "w")
  if not out then
    print("Error: could not open output file: " .. out_err)
    os.exit(1)
  end
  out:write(contents)
  out:close()
  io.stderr:write("Wrote " .. args.vendor_errors .. "\n")
end

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

-- Output grammar as JSON if requested
if args.json then
  local grammar = result

  -- Apply optimization unless disabled
  if not args.no_optimize then
    local optimize = require("pgen.optimize")
    grammar = optimize.optimize_grammar(grammar)
  end

  local cjson = require("cjson")
  print(cjson.encode(grammar))
  return
end

-- Compile the grammar
local output, err = pgen.compile(result, {
  parser_name = args.name,
  pgen_errors = args.pgen_errors,
  optimize = not args.no_optimize
})

if not output then
  print("Error generating parser: " .. (err or "unknown error"))
  os.exit(1)
end

-- If shared file option is provided, shell out to gcc to create shared object
if args.shared then
  local cc = os.getenv("PGEN_CC") or "gcc"
  local lua_cflags = os.getenv("PGEN_LUA_CFLAGS") or "`pkg-config --cflags lua5.1`"
  local lua_libs = os.getenv("PGEN_LUA_LIBS") or "`pkg-config --libs lua5.1`"
  local gcc_command = string.format("%s -shared -o %s -O3 -fPIC -x c - %s %s",
    cc, args.shared, lua_cflags, lua_libs)

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


local pgen = {}

-- Pattern types
local P, R, S, V, C, Ct, Cp, Cc = 1, 2, 3, 4, 5, 6, 7, 8

local mt = {}

local function make(t)
  return setmetatable(t, mt)
end

-- Helper to create a pattern object
local function pattern(type, value, name)
  return make{
    type = type,
    value = value,
    name = name
  }
end

local function coerce_pattern(value)
  local t = type(value)

  if t == "number" or t == "string" then
    return pgen.P(value)
  end

  return value
end

-- Create literal string pattern
function pgen.P(val)
  -- if it's negative number, conver it to -pgen.P(-n)
  if type(val) == "number" and val < 0 then
    return -pattern(P, -val)
  end

  return pattern(P, val)
end

-- Create character range pattern
function pgen.R(start, stop)
  return pattern(R, {start, stop})
end

-- Create character set pattern
function pgen.S(set)
  return pattern(S, set)
end

-- Reference to a named pattern
function pgen.V(name)
  return pattern(V, name)
end


-- Capture
function pgen.C(patt)
  return pattern(C, patt)
end

-- Capture table
function pgen.Ct(patt)
  return pattern(Ct, patt)
end

-- Capture position
function pgen.Cp()
  return pattern(Cp)
end

-- Constant capture
function pgen.Cc(...)
  return pattern(Cc, {...})
end

function mt.__add(a, b)
  return make{
    type = "choice",
    coerce_pattern(a), coerce_pattern(b)
  }
end

function mt.__sub(a, b)
  return -b * a

  -- return make{
  --   type = "except",
  --   coerce_pattern(a), coerce_pattern(b)
  -- }
end

function mt.__mul(a, b)
  return make{
    type = "sequence",
    coerce_pattern(a), coerce_pattern(b)
  }
end

-- negative predicate
function mt.__unm(a)
  return make{
    type = "negate",
    a
  }
end

-- Zero or more operator
function mt.__pow(a, n)
  return make{
    type = "repeat",
    a,
    n
  }
end

-- Compile grammar to C code
function pgen.compile(grammar, options)
  options = options or {}
  local parser_name = options.parser_name or "parser"

  local generator = require("pgen.generator")
  return generator.generate(grammar, parser_name)
end

-- this will require a module that returns a grammar, it will compile it to C
-- code, then compile it with gcc to temp file, then load the shared object as
-- a lua module, then return it
-- this does not cache the compiled code, so it will be recompiled every time require is called
function pgen.require(module_name, options)
  options = options or {}
  local show_timing = options.show_timing

  local socket
  if show_timing then
    socket = require("socket")
  end

  local function log_time(stage_name, start_time)
    if show_timing and socket then
      io.stderr:write(string.format("%s took %.2f seconds\n", stage_name, socket.gettime() - start_time))
    end
  end

  -- Load the grammar module
  local start_time = show_timing and socket.gettime()
  local grammar = require(module_name)
  log_time("Loading grammar module", start_time)

  local parser_name = "parser"

  -- Check if the result is a table (grammar)
  if type(grammar) ~= "table" then
    error("Error: Module must return a grammar table")
  end

  -- Compile the grammar into C code
  start_time = show_timing and socket.gettime()
  local output, err = pgen.compile(grammar, {
    -- TODO: generate non-conflicting parser name
    parser_name = parser_name
  })
  log_time("Compiling grammar to C code", start_time)

  if not output then
    error("Error generating parser: " .. (err or "unknown error"))
  end

  -- Compile the C code to a shared object using gcc, piping to stdin
  start_time = show_timing and socket.gettime()
  local tmp_so = os.tmpname() .. ".so"
  local gcc_command = string.format("gcc -shared -o %s -O3 -fPIC -x c - `pkg-config --cflags --libs lua5.1`", tmp_so)
  local gcc_process = io.popen(gcc_command, "w")
  if not gcc_process then
    error("Error: Could not execute gcc for compiling C code")
  end
  gcc_process:write(output)
  gcc_process:close()
  log_time("Compiling C code to shared object", start_time)

  -- Load the compiled shared object
  start_time = show_timing and socket.gettime()
  local parser = assert(package.loadlib(tmp_so, "luaopen_" .. parser_name))
  log_time("Loading compiled shared object", start_time)

  -- Cleanup temporary files
  os.remove(tmp_so)

  -- Return the parser module
  return parser()
end

return pgen

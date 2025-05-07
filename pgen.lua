local pgen = {}

-- Pattern types
local P, R, S, V = 1, 2, 3, 4

local make

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

local mt = {}

function make(t)
  return setmetatable(t, mt)
end

function mt.__add(a, b)
  return make{
    type = "choice",
    coerce_pattern(a), coerce_pattern(b)
  }
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
  local output_file = options.output_file or "parser.c"
  local parser_name = options.parser_name or "parser"

  local generator = require("pgen.generator")
  return generator.generate(grammar, parser_name, output_file)
end

return pgen

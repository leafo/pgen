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

-- Create literal string pattern
function pgen.P(val)
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

-- Sequence operator
local mt = {}

function make(t)
  return setmetatable(t, mt)
end

function mt.__add(a, b)
  return make{
    type = "sequence",
    a, b
  }
end

-- Choice operator
function mt.__mul(a, b)
  return make{
    type = "choice",
    a, b
  }
end

-- Optional operator
function mt.__unm(a)
  return make{
    type = "optional",
    a
  }
end

-- Zero or more operator
function mt.__pow(a, n)
  if n == 0 then
    return make{
      type = "star",
      a
    }
  else
    return make{
      type = "repeat",
      a,
      n
    }
  end
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

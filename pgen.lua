local pgen = {}

-- Pattern types
local types = require("pgen.types")

local mt = {}

local function make(t)
  return setmetatable(t, mt)
end

pgen._make = make

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

local function assert_pattern(obj)
  assert(getmetatable(obj) == mt, "Object is not a pattern")
  return obj
end

-- Create literal string pattern
function pgen.P(val)
  -- if it's negative number, conver it to -pgen.P(-n)
  if type(val) == "number" and val < 0 then
    return -pattern(types.P, -val)
  end

  return pattern(types.P, val)
end

-- Create character range pattern
-- Can handle multiple ranges: R("az", "AZ", "09")
function pgen.R(...)
  local ranges = {...}
  return pattern(types.R, ranges)
end

-- Create character set pattern
function pgen.S(set)
  return pattern(types.S, set)
end

-- Reference to a named pattern
function pgen.V(name)
  return pattern(types.V, name)
end


-- Capture
function pgen.C(patt)
  return pattern(types.C, coerce_pattern(patt))
end

-- Capture table
function pgen.Ct(patt)
  return pattern(types.Ct, assert_pattern(patt))
end

-- Capture position
function pgen.Cp()
  return pattern(types.Cp)
end

-- Constant capture
function pgen.Cc(...)
  local count = select("#", ...)
  return pattern(types.Cc, {..., count = count})
end

-- Lookahead pattern (matches without consuming input)
function pgen.L(patt)
  return pattern(types.L, coerce_pattern(patt))
end

-- Capture group (named capture for back-reference or named table fields)
function pgen.Cg(patt, name)
  assert(type(name) == "string", "Cg requires a string name")
  return pattern(types.Cg, coerce_pattern(patt), name)
end

-- Numbered capture (select nth capture from inner pattern)
function pgen.Cn(patt, n)
  assert(type(n) == "number", "Cn requires a number index")
  assert(n >= 0, "Cn index must be non-negative")
  assert(math.floor(n) == n, "Cn index must be an integer")
  return pattern(types.Cn, coerce_pattern(patt), n)
end

-- Capture match back (backreference - match value of named capture)
function pgen.Cmb(name)
  assert(type(name) == "string", "Cmb requires a string name")
  return pattern(types.Cmb, nil, name)
end

-- Match-time capture (evaluates Lua code during matching)
function pgen.Cmt(patt, code)
  assert(type(code) == "string", "Cmt requires a string of Lua code")
  return make{
    type = types.Cmt,
    value = coerce_pattern(patt),
    code = code
  }
end

-- Indenter: a match-time integer stack that lives alongside the parser, with
-- indentation-flavored operations. Backed by a stack in the generated C
-- parser; all operations are transactional (undone when the parser
-- backtracks past them). Each call to pgen.indenter() declares one stack.
--
-- Indent ops (measure the run of space/tab characters at the current
-- position; space = 1, tab = tab_width):
--   ind.check    consume the whitespace, succeed if width == top of stack
--   ind.advance  consume nothing, push width if width > top (fails otherwise)
--   ind.push     consume the whitespace, always push measured width
--   ind.prevent  consume nothing, push a sentinel so any nested advance fails
--   ind.pop      pop the stack (fails if empty)
--
-- Constant ops (consume nothing, for do-stack style flags):
--   ind.cpush(n)      push constant n
--   ind.ctop(cmp, n)  succeed if `top cmp n`; cmp is one of
--                     "eq", "ne", "lt", "le", "gt", "ge"
--
-- None of the operations produce captures.
local ctop_comparisons = {
  eq = true, ne = true, lt = true, le = true, gt = true, ge = true
}

function pgen.indenter(opts)
  opts = opts or {}

  assert(opts.tab_width == nil or (type(opts.tab_width) == "number" and opts.tab_width >= 0 and math.floor(opts.tab_width) == opts.tab_width),
    "indenter tab_width must be a non-negative integer")
  assert(opts.initial == nil or (type(opts.initial) == "number" and math.floor(opts.initial) == opts.initial),
    "indenter initial must be an integer")

  -- Plain descriptor table shared by every op node from this indenter.
  -- Descriptor identity is what maps ops to the same stack; the generator
  -- assigns the actual stack id at compile time.
  local descriptor = {
    tab_width = opts.tab_width or 4,
    initial = opts.initial or 0
  }

  local function op(fields)
    fields.type = types.Ind
    fields.indenter = descriptor
    return make(fields)
  end

  return {
    descriptor = descriptor,

    check = op{op = "check"},
    advance = op{op = "advance"},
    push = op{op = "push"},
    prevent = op{op = "prevent"},
    pop = op{op = "pop"},

    cpush = function(n)
      assert(type(n) == "number" and math.floor(n) == n, "cpush requires an integer value")
      return op{op = "cpush", value = n}
    end,

    ctop = function(cmp, n)
      assert(ctop_comparisons[cmp], "ctop comparison must be one of eq, ne, lt, le, gt, ge")
      assert(type(n) == "number" and math.floor(n) == n, "ctop requires an integer value")
      return op{op = "ctop", cmp = cmp, value = n}
    end
  }
end

-- Throw labeled failure
function pgen.T(label)
  assert(type(label) == "string", "T requires a string label")
  return pattern(types.T, label)
end

function mt.__add(a, b)
  return make{
    type = "choice",
    coerce_pattern(a), coerce_pattern(b)
  }
end

function mt.__sub(a, b)
  return -b * a
end

function mt.__mul(a, b)
  a = coerce_pattern(a)
  b = coerce_pattern(b)

  local sequence = {
    type = "sequence"
  }

  if a.type == "sequence" then
    for i, p in ipairs(a) do
      table.insert(sequence, p)
    end
  else
    table.insert(sequence, a)
  end

  if b.type == "sequence" then
    for i, p in ipairs(b) do
      table.insert(sequence, p)
    end
  else
    table.insert(sequence, b)
  end

  return make(sequence)
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

-- Length operator for lookahead pattern (matches without consuming input)
function mt.__len(a)
  return pgen.L(a)
end

-- Division operator for numbered capture
function mt.__div(a, b)
  if type(b) == "number" then
    return pgen.Cn(a, b)
  end
  error("Pattern division only supports number argument (numbered capture)")
end


-- Compile grammar to C code
function pgen.compile(grammar, options)
  options = options or {}
  local parser_name = options.parser_name or "parser"

  -- Apply optimizations before generation (unless disabled)
  if options.optimize ~= false then
    local optimize = require("pgen.optimize")
    grammar = optimize.optimize_grammar(grammar)
  end

  local generator = require("pgen.generator")
  return generator.generate(grammar, parser_name, {
    pgen_errors = options.pgen_errors,
    max_depth = options.max_depth
  })
end

-- this will require a module that returns a grammar, it will compile it to C
-- code, then compile it with gcc to temp file, then load the shared object as
-- a lua module, then return it
-- this does not cache the compiled code, so it will be recompiled every time require is called
function pgen.require(module_name, options)
  options = options or {}
  local show_timing = options.show_timing
  local parser_name = options.parser_name or "parser"

  local socket
  if show_timing then
    socket = require("socket")
  end

  local function log_time(stage_name, start_time)
    if show_timing and socket then
      io.stderr:write(string.format("[%.2fs] %s\n", socket.gettime() - start_time, stage_name))
    end
  end

  -- Load the grammar module
  local start_time = show_timing and socket.gettime()
  local grammar = require(module_name)
  log_time("Loaded grammar module", start_time)

  -- Check if the result is a table (grammar)
  if type(grammar) ~= "table" then
    error("Error: Module must return a grammar table")
  end

  -- Apply transform if provided
  if options.transform then
    start_time = show_timing and socket.gettime()
    grammar = assert(options.transform(grammar))
    log_time("Applied grammar transform", start_time)
  end

  -- Compile the grammar into C code
  start_time = show_timing and socket.gettime()
  local output, err = pgen.compile(grammar, {
    -- TODO: generate non-conflicting parser name
    parser_name = parser_name,
    optimize = options.optimize,
    max_depth = options.max_depth
  })
  log_time("Compiled grammar to C code (" .. tostring(#output) .. " bytes)", start_time)

  if not output then
    error("Error generating parser: " .. (err or "unknown error"))
  end

  -- Compile the C code to a shared object using gcc, piping to stdin
  start_time = show_timing and socket.gettime()
  local tmp_so = os.tmpname() .. ".so"
  local gcc_command

  if options.debug then
    gcc_command = string.format("gcc -shared -o %s -g -O0 -fno-omit-frame-pointer -fPIC -x c - `pkg-config --cflags --libs lua5.1`", tmp_so)
  else
    gcc_command = string.format("gcc -shared -o %s -march=native -flto -finline-functions -O3 -fPIC -x c - `pkg-config --cflags --libs lua5.1`", tmp_so)
  end

  local gcc_process = io.popen(gcc_command, "w")
  if not gcc_process then
    error("Error: Could not execute gcc for compiling C code")
  end
  gcc_process:write(output)
  gcc_process:close()
  log_time("Compiled C code to shared object", start_time)

  -- Load the compiled shared object
  start_time = show_timing and socket.gettime()
  local parser = assert(package.loadlib(tmp_so, "luaopen_" .. parser_name))
  log_time("Loaded compiled shared object", start_time)

  -- Cleanup temporary files
  os.remove(tmp_so)

  -- Return the parser module
  return parser()
end

return pgen

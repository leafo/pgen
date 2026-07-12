local generator = {}
local types = require("pgen.types")

-- human readable version of the string that can be embeded into comments
local function escape_string(str)
  return require("cjson").encode(str)
end

local function escape_c_literal(str, delim)
  delim = delim or '"'

  -- Mapping for characters with standard C escape sequences or requiring escaping
  local escapes = {
    [string.char(0)]  = "\\0",  -- Null
    [string.char(7)]  = "\\a",  -- Bell (Alert)
    [string.char(8)]  = "\\b",  -- Backspace
    [string.char(9)]  = "\\t",  -- Horizontal Tab
    [string.char(10)] = "\\n",  -- Newline (Line Feed)
    [string.char(11)] = "\\v",  -- Vertical Tab
    [string.char(12)] = "\\f",  -- Form Feed
    [string.char(13)] = "\\r",  -- Carriage Return
    ['"']             = "\\\"", -- Double quote
    ["\\"]            = "\\\\"  -- Backslash
    -- Question mark ('?') sometimes needs escaping to avoid trigraphs, but rare: ["?"] = "\\?"
  }

  -- Use gsub with a function to process each character
  local escaped = str:gsub(".", function(c)
    -- Check if the character has a predefined escape sequence
    if escapes[c] then
      return escapes[c]
    end

    -- Check if the character is printable ASCII (ASCII 32 to 126)
    local byte_val = string.byte(c)
    if byte_val >= 32 and byte_val <= 126 then
      return c -- Printable characters are returned as is
    end

    -- For any other character (non-printable ASCII or outside ASCII range),
    -- use octal escape sequence. This safely handles any byte value.
    return string.format("\\%03o", byte_val)
  end)

  -- Enclose the escaped string in double quotes for C literal
  return delim .. escaped .. delim
end

local function assert_valid_c_identifier(name)
  if not name:match("^[A-Za-z_][A-Za-z0-9_]*$") then
    error("Invalid capture name for C identifier: " .. escape_string(name))
  end
end

-- Collect all Cg and Cmb names from a grammar (both use sentinels)
local function collect_cg_names(grammar)
  local visitor = require("pgen.visitor")
  local names = {}
  visitor.visit_grammar(grammar, function(node)
    if node.type == types.Cg or node.type == types.Cmb then
      names[node.name] = true
    end
  end)
  -- Convert to sorted array for deterministic output
  local result = {}
  for name in pairs(names) do
    table.insert(result, name)
  end
  table.sort(result)
  return result
end

-- Collect all unique non-nil values from Cc nodes in a grammar. These are
-- interned into the Lua registry once at module load; capture-log CONST
-- entries reference them by registry ref, so matching never constructs
-- constant values. Returns a deterministically ordered array of values and
-- a value -> 0-based index map.
local function collect_constants(grammar)
  local visitor = require("pgen.visitor")
  local seen = {}
  visitor.visit_grammar(grammar, function(node)
    if node.type == types.Cc then
      local values = node.value
      for i = 1, values.count do
        if values[i] ~= nil then
          seen[values[i]] = true
        end
      end
    end
  end)

  local strings, numbers = {}, {}
  local has_false, has_true = false, false
  for value in pairs(seen) do
    local t = type(value)
    if t == "string" then
      table.insert(strings, value)
    elseif t == "number" then
      table.insert(numbers, value)
    elseif t == "boolean" then
      if value then has_true = true else has_false = true end
    end
  end
  table.sort(strings)
  table.sort(numbers)

  local pool, index = {}, {}
  local function add(value)
    table.insert(pool, value)
    index[value] = #pool - 1
  end
  for _, value in ipairs(strings) do add(value) end
  for _, value in ipairs(numbers) do add(value) end
  if has_false then add(false) end
  if has_true then add(true) end

  return pool, index
end

-- Collect all Cmt and Cfn nodes from a grammar, assigning each a unique ID
-- into a shared callback registry. Returns array of {id, code, kind} for
-- unique codes, and a new grammar with cmt_id fields set. Identical code
-- strings of the same kind share an ID; the kinds are registered separately
-- because their load conventions differ (a Cmt chunk IS the callback, while
-- a Cfn chunk is run once and must return the callback).
local function collect_cmt_codes(grammar)
  local visitor = require("pgen.visitor")
  local pgen = require("pgen")
  local codes = {}           -- Array of {id, code, kind} for unique codes only
  local code_to_id = {}      -- Map: kind-tagged code string -> id (deduplication)
  local next_id = 0

  local new_grammar = visitor.visit_grammar(grammar, function(node, replace)
    if (node.type == types.Cmt or node.type == types.Cfn) and node.cmt_id == nil then
      local kind = node.type == types.Cmt and "cmt" or "cfn"
      local code = node.code
      local key = kind .. "\0" .. code
      local id = code_to_id[key]

      if id == nil then
        -- First time seeing this code, assign new ID
        id = next_id
        code_to_id[key] = id
        table.insert(codes, {id = id, code = code, kind = kind})
        next_id = next_id + 1
      end

      -- Create a copy of the node with cmt_id assigned (possibly shared)
      replace(visitor.copy_node(node, {cmt_id = id}))
    end
  end)

  return codes, new_grammar
end

-- Collect all indenter descriptors referenced by Ind nodes in a grammar,
-- assigning each distinct descriptor (by table identity) a stack id.
-- Rules are visited in sorted order so ids are deterministic regardless of
-- table iteration order.
-- Returns array of {id, tab_width, initial} and a new grammar with stack_id
-- set on every Ind node.
local function collect_indenters(grammar)
  local visitor = require("pgen.visitor")
  local descriptors = {}
  local desc_to_id = {}

  local names = {}
  for name in pairs(grammar) do
    table.insert(names, name)
  end
  table.sort(names, function(a, b)
    return tostring(a) < tostring(b)
  end)

  local new_grammar = {}
  local changed = false

  for _, name in ipairs(names) do
    local pattern = grammar[name]
    if type(pattern) == "table" then
      local new_pattern = visitor.visit_pattern(pattern, function(node, replace)
        if node.type == types.Ind then
          local desc = node.indenter
          if type(desc) ~= "table" then
            error("Ind node is missing its indenter descriptor")
          end
          local id = desc_to_id[desc]
          if id == nil then
            id = #descriptors
            desc_to_id[desc] = id
            table.insert(descriptors, {
              id = id,
              tab_width = desc.tab_width or 4,
              initial = desc.initial or 0
            })
          end
          if node.stack_id ~= id then
            replace(visitor.copy_node(node, {stack_id = id}))
          end
        end
      end)
      new_grammar[name] = new_pattern
      if new_pattern ~= pattern then changed = true end
    else
      new_grammar[name] = pattern
    end
  end

  if #descriptors > 256 then
    error("Too many indenter stacks in grammar (max 256)")
  end

  return descriptors, changed and new_grammar or grammar
end

local function template_code(template, vars)
  -- Use gsub to find placeholders like $VARNAME
  -- The pattern matches '$' followed by UPPER_CASE letters
  local result = template:gsub("%$([A-Z_]+)%$", function(var_name)
    -- Look up the captured variable name in the vars table
    local value = vars[var_name]
    -- If the variable exists in the table, return its value (converted to string)
    if value ~= nil then
      return tostring(value)
    else
      -- If the variable is not found, return the original placeholder string
      -- (e.g., "$UNDEFINED_VAR") so it's not replaced.
      return "$" .. var_name .. "$"
    end
  end)
  return result
end

-- Iterator for rules, sorted with start_rule first, then alphabetically
local function sorted_rules(rules, start_rule)
  local names = {}
  for name in pairs(rules) do
    if name ~= start_rule then
      table.insert(names, name)
    end
  end
  table.sort(names)
  table.insert(names, 1, start_rule)
  local i = 0
  return function()
    i = i + 1
    local name = names[i]
    if name then
      return name, rules[name]
    end
  end
end

local function position_operations(pattern, context)
  local needs_state = context.analyze.changes_backtrack_state(
    pattern, context.rules, context.stateful_rules)
  if needs_state then
    return "REMEMBER_POSITION(parser, pos);", "RESTORE_POSITION(parser, pos);"
  end
  return "REMEMBER_INPUT_POSITION(parser, pos);", "RESTORE_INPUT_POSITION(parser, pos);"
end

-- Compile a grammar definition to C code
function generator.generate(grammar, parser_name, options)
  options = options or {}

  -- Collect Cmt codes and get transformed grammar with cmt_id assigned
  local cmt_codes, transformed_grammar = collect_cmt_codes(grammar)

  -- Collect indenter descriptors and assign stack ids to Ind nodes
  local indenters
  indenters, transformed_grammar = collect_indenters(transformed_grammar)

  local rules = {}
  local start_rule = nil

  -- Extract rules from transformed grammar (which has cmt_id on Cmt nodes)
  for name, pattern in pairs(transformed_grammar) do
    local skip = false
    if not start_rule then
      if type(pattern) == "string" and name == 1 then
        start_rule = pattern
        skip = true
      else
        start_rule = name
      end
    end
    if not skip then
      rules[name] = pattern
    end
  end

  if not start_rule then
    error("Grammar does not contain a starting rule")
  end

  -- Reject unbounded repetitions whose body can match the empty string,
  -- which would loop forever at parse time
  local analyze = require("pgen.analyze")
  analyze.check_loops(rules)

  -- Collect all Cg (capture group) names for sentinel generation
  local cg_names = collect_cg_names(transformed_grammar)
  for _, name in ipairs(cg_names) do
    assert_valid_c_identifier(name)
  end

  -- Collect Cc constants for load-time interning
  local const_pool, const_index = collect_constants(transformed_grammar)
  local has_consts = #const_pool > 0 or #cg_names > 0

  -- Generate the C code
  local c_chunks = {
    generator.generate_parser_header(parser_name, cg_names, cmt_codes, indenters, const_pool),
    generator.generate_forward_declarations(rules, start_rule),
    generator.generate_rule_functions(rules, start_rule, const_index, cg_names),
    generator.generate_parser_main(parser_name, start_rule, cmt_codes, indenters, has_consts),
    -- Add compilation instructions as a comment
    template_code([[/*
To compile as a Lua module:
gcc -shared -o $PARSER_NAME$.so -fPIC $PARSER_NAME$.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local $PARSER_NAME$ = require "$PARSER_NAME$"
local result = $PARSER_NAME$.parse("your input string")
*/
]], {PARSER_NAME = parser_name})
  }

  if options.pgen_debug then
    table.insert(c_chunks, 1, "#define PGEN_DEBUG 1")
  end

  if options.pgen_errors then
    table.insert(c_chunks, 1, "#define PGEN_ERRORS 1")
  end

  if options.max_depth then
    assert(type(options.max_depth) == "number" and options.max_depth >= 1,
      "max_depth must be a positive number")
    table.insert(c_chunks, 1, "#define PGEN_MAX_DEPTH " .. math.floor(options.max_depth))
  end

  return table.concat(c_chunks, "\n")
end

-- Generate the Cg name table: capture-log GROUP entries carry the name as
-- an index into this table; __cg_name_refs holds registry refs for the
-- interned name strings (used as table keys during materialization)
local function generate_cg_names(cg_names)
  local lines = {}

  if #cg_names > 0 then
    table.insert(lines, "// Named capture group names (GROUP entry aux = index here)")
    table.insert(lines, "static const char *__cg_names[] = {")
    for _, name in ipairs(cg_names) do
      table.insert(lines, "  " .. escape_c_literal(name) .. ",")
    end
    table.insert(lines, "  NULL  // terminator")
    table.insert(lines, "};")
    table.insert(lines, "")
    table.insert(lines, template_code(
      "static int __cg_name_refs[$COUNT$];", {COUNT = #cg_names}))
  else
    table.insert(lines, "// No named capture groups")
    table.insert(lines, "static int __cg_name_refs[1];")
  end

  return table.concat(lines, "\n") .. "\n"
end

-- Generate the capture-log evaluator: materializes log entries into Lua
-- values. Runs once after a successful parse (and on demand at Cmt
-- boundaries), so it is not on the matching hot path.
local function generate_cap_evaluator()
  return [[
// --- Capture log evaluation ---

static int pgen_cap_eval(Parser *parser, size_t *i);

// Push the single value a capture group produces: its first inner capture
// value, or the text it matched when its contents produce no values
static void pgen_cap_eval_group(Parser *parser, size_t *i) {
  size_t open = *i;
  pgen_cap_skip(parser, i);  // *i now points past GROUP_CLOSE
  size_t close = *i - 1;

  size_t j = open + 1;
  while (j < close) {
    int produced = pgen_cap_eval(parser, &j);
    if (produced > 0) {
      if (produced > 1) {
        // keep only the first value
        lua_pop(parser->L, produced - 1);
        parser->top -= produced - 1;
      }
      return;
    }
  }

  // no values: the group's value is the text it matched
  size_t start = parser->caps[open].start;
  pgen_checkstack(parser, 1);
  lua_pushlstring(parser->L, parser->input + start, parser->caps[close].start - start);
  parser->top++;
}

// Materialize one log item (entry or bracketed range) at *i, advancing *i
// past the item. Returns the number of Lua values pushed: always 1 except
// for transform captures, whose callbacks may return any number of values.
static int pgen_cap_eval(Parser *parser, size_t *i) {
  PgenCap *cap = &parser->caps[*i];
  switch (cap->kind) {
  case PGEN_CAP_STR:
    pgen_checkstack(parser, 1);
    lua_pushlstring(parser->L, parser->input + cap->start, cap->len);
    parser->top++;
    (*i)++;
    return 1;
  case PGEN_CAP_CONST:
    pgen_checkstack(parser, 1);
    lua_rawgeti(parser->L, LUA_REGISTRYINDEX, cap->aux);
    parser->top++;
    (*i)++;
    return 1;
  case PGEN_CAP_NIL:
    pgen_checkstack(parser, 1);
    lua_pushnil(parser->L);
    parser->top++;
    (*i)++;
    return 1;
  case PGEN_CAP_POS:
    pgen_checkstack(parser, 1);
    lua_pushinteger(parser->L, (lua_Integer)(cap->start + 1));
    parser->top++;
    (*i)++;
    return 1;
  case PGEN_CAP_VALUE:
    pgen_checkstack(parser, 1);
    lua_pushvalue(parser->L, cap->aux);
    parser->top++;
    (*i)++;
    return 1;
  case PGEN_CAP_GROUP_OPEN:
    pgen_cap_eval_group(parser, i);
    return 1;
  case PGEN_CAP_FN_OPEN: {
    // Transform capture: inner values become arguments, the callback's
    // return values become the capture values (innermost-first order falls
    // out of the recursion here)
    size_t open = *i;
    int func_base = parser->top;
    pgen_checkstack(parser, 1);
    lua_rawgeti(parser->L, LUA_REGISTRYINDEX, cap->aux);
    parser->top++;

    int nargs = 0;
    size_t j = open + 1;
    while (parser->caps[j].kind != PGEN_CAP_FN_CLOSE) {
      if (parser->caps[j].kind == PGEN_CAP_GROUP_OPEN) {
        // named groups are not visible as arguments (as at the top level)
        pgen_cap_skip(parser, &j);
      } else {
        nargs += pgen_cap_eval(parser, &j);
      }
    }

    if (nargs == 0) {
      // no inner captures: the callback receives the matched text
      size_t start = parser->caps[open].start;
      pgen_checkstack(parser, 1);
      lua_pushlstring(parser->L, parser->input + start, parser->caps[j].start - start);
      nargs = 1;
    }

    // lua_call propagates errors (aborts materialization on Lua error)
    lua_call(parser->L, nargs, LUA_MULTRET);
    parser->top = lua_gettop(parser->L);
    if (parser->stack_claimed > parser->top) parser->stack_claimed = parser->top;

    *i = j + 1;  // past FN_CLOSE
    return parser->top - func_base;
  }
  default: {  // PGEN_CAP_TBL_OPEN
    // No presizing: counting items would re-walk every nested subtree at
    // each nesting level, which costs more than letting the table grow
    pgen_checkstack(parser, 3);
    lua_createtable(parser->L, 0, 0);
    parser->top++;
    int table_idx = parser->top;

    size_t j = *i + 1;
    int array_idx = 1;
    while (parser->caps[j].kind != PGEN_CAP_TBL_CLOSE) {
      if (parser->caps[j].kind == PGEN_CAP_GROUP_OPEN) {
        pgen_checkstack(parser, 2);
        lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cg_name_refs[parser->caps[j].aux]);
        parser->top++;
        pgen_cap_eval_group(parser, &j);
        lua_rawset(parser->L, table_idx);
        parser->top -= 2;
      } else {
        // rawseti pops the top value, so multi-value items assign their
        // indexes in reverse
        int produced = pgen_cap_eval(parser, &j);
        for (int v = produced - 1; v >= 0; v--) {
          lua_rawseti(parser->L, table_idx, array_idx + v);
        }
        array_idx += produced;
        parser->top -= produced;
      }
    }
    *i = j + 1;  // past TBL_CLOSE
    return 1;
  }
  }
}

// Run a match-time capture: materialize the inner captures, call the
// callback with (subject, pos, ...captures), and interpret its results per
// lpeg semantics: position/true = success, false/nil = failure, extra
// return values become captures (kept on the Lua stack)
static void pgen_run_cmt(Parser *parser, int func_ref, size_t start_pos, size_t cap_base, int top_base) {
  lua_State *L = parser->L;
  size_t pos_after_inner = parser->pos;
  int leftovers = parser->top - top_base;  // nested Cmt values still on the stack

  pgen_checkstack(parser, 3);
  lua_rawgeti(L, LUA_REGISTRYINDEX, func_ref);
  lua_pushlstring(L, parser->input, parser->input_len);
  lua_pushinteger(L, (lua_Integer)(pos_after_inner + 1));  // 1-based
  parser->top += 3;

  int nargs = 2;
  size_t i = cap_base;
  while (i < parser->cap_len) {
    if (parser->caps[i].kind == PGEN_CAP_GROUP_OPEN) {
      // named groups only matter inside Ct; they aren't passed as arguments
      pgen_cap_skip(parser, &i);
    } else {
      nargs += pgen_cap_eval(parser, &i);
    }
  }
  parser->cap_len = cap_base;  // consume the inner captures

  // lua_call propagates errors (aborts parse on Lua error)
  lua_call(L, nargs, LUA_MULTRET);
  parser->top = lua_gettop(L);
  if (parser->stack_claimed > parser->top) parser->stack_claimed = parser->top;

  int returns_count = parser->top - (top_base + leftovers);

  if (returns_count == 0) {
    // No return value = match fails
    parser->success = false;
    PGEN_RECORD_FURTHEST(parser);  // record at pos_after_inner, before rewind
    parser->pos = start_pos;
  } else {
    int first = top_base + leftovers + 1;
    int first_type = lua_type(L, first);
    if (first_type == LUA_TNUMBER) {
      // Number = new position (1-based from Lua)
      lua_Integer new_pos = lua_tointeger(L, first) - 1;
      // Per lpeg: must be in range [pos_after_inner, input_len]
      if (new_pos >= (lua_Integer)pos_after_inner && new_pos <= (lua_Integer)parser->input_len) {
        parser->pos = (size_t)new_pos;
        parser->success = true;
      } else {
        parser->success = false;
        PGEN_RECORD_FURTHEST(parser);
        parser->pos = start_pos;
      }
    } else if (first_type == LUA_TBOOLEAN && lua_toboolean(L, first)) {
      // true = succeed without consuming (position stays at pos_after_inner)
      parser->success = true;
    } else {
      // false, nil, or other = fail
      parser->success = false;
      PGEN_RECORD_FURTHEST(parser);
      parser->pos = start_pos;
    }
  }

  if (parser->success && returns_count > 1) {
    // Drop leftover nested-Cmt slots and the first return value; the
    // remaining returns stay on the stack as the new captures
    for (int r = 0; r < leftovers + 1; r++) {
      lua_remove(L, top_base + 1);
    }
    parser->top -= leftovers + 1;
    if (parser->stack_claimed > parser->top) parser->stack_claimed = parser->top;
    int extras = returns_count - 1;
    for (int r = 0; r < extras; r++) {
      pgen_cap_push(parser, PGEN_CAP_VALUE, top_base + 1 + r, 0, 0);
    }
  } else {
    PGEN_SETTOP(parser, top_base);
  }
}
]]
end

-- Generate Cmt (match-time capture) infrastructure
-- Returns C code for: static code strings, ref array, and init function
local function generate_cmt_infrastructure(cmt_codes)
  if #cmt_codes == 0 then
    return ""
  end

  local lines = {}
  table.insert(lines, "// Callback (Cmt/Cfn) infrastructure")
  table.insert(lines, "")

  -- Generate static code strings
  for _, cmt in ipairs(cmt_codes) do
    table.insert(lines, template_code(
      "static const char __cmt_code_$ID$[] = $CODE_STR$;",
      {ID = cmt.id, CODE_STR = escape_c_literal(cmt.code)}
    ))
  end

  -- Generate refs array
  table.insert(lines, "")
  table.insert(lines, template_code("static int __cmt_refs[$COUNT$];", {COUNT = #cmt_codes}))

  -- Generate init function
  table.insert(lines, "")
  table.insert(lines, [[// Initialize callbacks by loading their Lua code
static void __cmt_init(lua_State *L) {]])

  for _, cmt in ipairs(cmt_codes) do
    if cmt.kind == "cfn" then
      -- Cfn: the chunk runs once at load and must return the callback
      table.insert(lines, template_code([[  if (luaL_loadstring(L, __cmt_code_$ID$) != 0) {
    luaL_error(L, "Failed to load Cfn callback $ID$: %s", lua_tostring(L, -1));
  }
  if (lua_pcall(L, 0, 1, 0) != 0) {
    luaL_error(L, "Failed to run Cfn chunk $ID$: %s", lua_tostring(L, -1));
  }
  if (!lua_isfunction(L, -1)) {
    luaL_error(L, "Cfn chunk $ID$ did not return a function");
  }
  __cmt_refs[$ID$] = luaL_ref(L, LUA_REGISTRYINDEX);]], {ID = cmt.id}))
    else
      table.insert(lines, template_code([[  if (luaL_loadstring(L, __cmt_code_$ID$) != 0) {
    luaL_error(L, "Failed to load Cmt callback $ID$: %s", lua_tostring(L, -1));
  }
  __cmt_refs[$ID$] = luaL_ref(L, LUA_REGISTRYINDEX);]], {ID = cmt.id}))
    end
  end

  table.insert(lines, "}")
  table.insert(lines, "")

  return table.concat(lines, "\n")
end

-- Generate the constant interning infrastructure: a registry ref array for
-- Cc values plus an init function that interns them (and the Cg group
-- names) once at module load
local function generate_const_infrastructure(const_pool, cg_names)
  if #const_pool == 0 and #cg_names == 0 then
    return ""
  end

  local lines = {}
  table.insert(lines, "// Interned constants (pushed once at module load)")

  if #const_pool > 0 then
    table.insert(lines, template_code("static int __const_refs[$COUNT$];", {COUNT = #const_pool}))
  end

  table.insert(lines, "")
  table.insert(lines, "static void __const_init(lua_State *L) {")

  for i, value in ipairs(const_pool) do
    local t = type(value)
    local push_line
    if t == "string" then
      push_line = template_code("  lua_pushlstring(L, $VALUE$, $LENGTH$);", {
        VALUE = escape_c_literal(value),
        LENGTH = #value
      })
    elseif t == "number" then
      push_line = template_code("  lua_pushnumber(L, $VALUE$);", {
        VALUE = string.format("%.17g", value)
      })
    else -- boolean
      push_line = "  lua_pushboolean(L, " .. (value and "1" or "0") .. ");"
    end
    table.insert(lines, push_line .. template_code([[

  __const_refs[$IDX$] = luaL_ref(L, LUA_REGISTRYINDEX);]], {IDX = i - 1}))
  end

  if #cg_names > 0 then
    table.insert(lines, [[  for (int i = 0; __cg_names[i] != NULL; i++) {
    lua_pushstring(L, __cg_names[i]);
    __cg_name_refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
  }]])
  end

  table.insert(lines, "}")
  table.insert(lines, "")

  return table.concat(lines, "\n")
end

-- Generate indenter stack/trail infrastructure snippets for the parser header
-- Returns a table of template variable values; all empty strings when the
-- grammar uses no indenters so the generated code is unchanged.
local function generate_indenter_header_vars(indenters)
  if #indenters == 0 then
    return {
      IND_TYPES = "",
      IND_PARSER_FIELDS = "",
      IND_PP_FIELD = "",
      IND_REMEMBER = "",
      IND_RESTORE = "",
      IND_HELPERS = ""
    }
  end

  local ind_types = template_code([[#include <limits.h>

// Indenter (match-time integer stack) infrastructure
#define PGEN_HAS_IND 1
#define PGEN_IND_STACK_COUNT $COUNT$
// Sentinel pushed by `prevent`: no measured width compares greater than it,
// so any nested `advance` fails
#define PGEN_IND_PREVENT_SENTINEL INT_MAX

typedef struct {
  int *items;
  int size;
  int cap;
} PgenIndStack;

// Undo log entry for transactional stack operations. Rewinding the trail on
// backtrack reverses every push/pop performed since the choice point.
typedef struct {
  unsigned char stack_id;
  unsigned char op;   // 0 = push, 1 = pop
  int value;          // for pop entries: the value that was popped
} PgenTrailEntry;

]], {COUNT = #indenters})

  local ind_helpers = [[
// Rewind the indenter trail to a previous length, undoing pushes and pops
static void pgen_ind_trail_rewind(Parser *parser, size_t index) {
  while (parser->trail_len > index) {
    parser->trail_len--;
    PgenTrailEntry *e = &parser->trail[parser->trail_len];
    PgenIndStack *s = &parser->ind_stacks[e->stack_id];
    if (e->op == 0) {
      // undo push
      s->size--;
    } else {
      // undo pop: the slot is still allocated, restore the value
      s->items[s->size] = e->value;
      s->size++;
    }
  }
}

static void pgen_ind_trail_record(Parser *parser, int stack_id, int op, int value) {
  if (parser->trail_len >= parser->trail_cap) {
    size_t new_cap = parser->trail_cap == 0 ? 64 : parser->trail_cap * 2;
    PgenTrailEntry *trail = (PgenTrailEntry*)realloc(parser->trail, new_cap * sizeof(PgenTrailEntry));
    if (!trail) {
      luaL_error(parser->L, "pgen: out of memory growing indenter trail");
    }
    parser->trail = trail;
    parser->trail_cap = new_cap;
  }
  parser->trail[parser->trail_len].stack_id = (unsigned char)stack_id;
  parser->trail[parser->trail_len].op = (unsigned char)op;
  parser->trail[parser->trail_len].value = value;
  parser->trail_len++;
}

static void pgen_ind_push(Parser *parser, int stack_id, int value) {
  PgenIndStack *s = &parser->ind_stacks[stack_id];
  if (s->size >= s->cap) {
    int new_cap = s->cap * 2;
    int *items = (int*)realloc(s->items, new_cap * sizeof(int));
    if (!items) {
      luaL_error(parser->L, "pgen: out of memory growing indenter stack");
    }
    s->items = items;
    s->cap = new_cap;
  }
  s->items[s->size++] = value;
  pgen_ind_trail_record(parser, stack_id, 0, value);
}

// Pop the stack; returns false if the stack is empty
static bool pgen_ind_pop(Parser *parser, int stack_id) {
  PgenIndStack *s = &parser->ind_stacks[stack_id];
  if (s->size == 0) {
    return false;
  }
  s->size--;
  pgen_ind_trail_record(parser, stack_id, 1, s->items[s->size]);
  return true;
}

// Measure the indentation width of the run of space/tab characters at the
// current position (space = 1, tab = tab_width). Sets *end_pos to the first
// position past the run.
static int pgen_ind_measure(Parser *parser, size_t *end_pos, int tab_width) {
  size_t p = parser->pos;
  int width = 0;
  while (p < parser->input_len) {
    char c = parser->input[p];
    if (c == ' ') {
      width += 1;
    } else if (c == '\t') {
      width += tab_width;
    } else {
      break;
    }
    p++;
  }
  *end_pos = p;
  return width;
}

]]

  return {
    IND_TYPES = ind_types,
    IND_PARSER_FIELDS = [[

  PgenIndStack ind_stacks[PGEN_IND_STACK_COUNT];
  PgenTrailEntry *trail;
  size_t trail_len;
  size_t trail_cap;]],
    IND_PP_FIELD = "\n  size_t trail_index;",
    -- these extend the line-continuation macros, so they must supply their
    -- own leading " \" on the previous line
    IND_REMEMBER = " \\\n  (pp).trail_index = (parser)->trail_len;",
    IND_RESTORE = " \\\n  pgen_ind_trail_rewind(parser, (pp).trail_index);",
    IND_HELPERS = ind_helpers
  }
end

-- Generate parser header
function generator.generate_parser_header(parser_name, cg_names, cmt_codes, indenters, const_pool)
  cg_names = cg_names or {}
  indenters = indenters or {}

  local header_vars = generate_indenter_header_vars(indenters)
  header_vars.PARSER_NAME = parser_name

  return template_code([[#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <assert.h>

// $PARSER_NAME$ - generated parser

// Maximum rule-call recursion depth before the parse is aborted with a Lua
// error (prevents C stack overflow on deeply nested input). Override with
// the max_depth compile option or -DPGEN_MAX_DEPTH=n
#ifndef PGEN_MAX_DEPTH
#define PGEN_MAX_DEPTH 5000
#endif

// --- Capture log ---
// Captures are recorded as log entries during matching and only materialized
// into Lua values after the whole parse succeeds. Backtracking rewinds the
// log length, so discarded speculative captures never touch the Lua runtime.
// The exception is Cmt: its callback runs mid-parse and its extra return
// values live on the Lua stack, referenced by PGEN_CAP_VALUE entries.
enum {
  PGEN_CAP_STR,         // start/len: slice of the input
  PGEN_CAP_CONST,       // aux: registry ref of an interned constant
  PGEN_CAP_NIL,
  PGEN_CAP_POS,         // start: input position
  PGEN_CAP_VALUE,       // aux: absolute Lua stack index (Cmt results)
  PGEN_CAP_TBL_OPEN,    // Ct brackets
  PGEN_CAP_TBL_CLOSE,
  PGEN_CAP_GROUP_OPEN,  // Cg brackets; aux: name index, start: input position
  PGEN_CAP_GROUP_CLOSE,
  PGEN_CAP_FN_OPEN,     // Cfn brackets; aux: callback registry ref, start: pos
  PGEN_CAP_FN_CLOSE
};

// Bracket kind tests: OPEN kinds and their CLOSE kinds are laid out in
// matching order after the scalar kinds
#define PGEN_CAP_IS_OPEN(k) \
  ((k) == PGEN_CAP_TBL_OPEN || (k) == PGEN_CAP_GROUP_OPEN || (k) == PGEN_CAP_FN_OPEN)
#define PGEN_CAP_IS_CLOSE(k) \
  ((k) == PGEN_CAP_TBL_CLOSE || (k) == PGEN_CAP_GROUP_CLOSE || (k) == PGEN_CAP_FN_CLOSE)

typedef struct {
  int kind;
  int aux;
  size_t start;
  size_t len;
} PgenCap;

$IND_TYPES$typedef struct {
  const char *input;
  size_t input_len;
  size_t pos;
  bool success;
  char error_message[256];
  const char *throw_label;  // Label from T() or NULL for ordinary failure
  size_t throw_pos;         // Position where T() was thrown
  size_t furthest_fail;     // Furthest position where a match attempt failed
  size_t depth;
  int top;                  // Shadow of lua_gettop(L), exact between patterns
  int stack_claimed;        // Stack index secured so far via lua_checkstack
  PgenCap *caps;            // Capture log
  size_t cap_len;
  size_t cap_cap;
  lua_State *L;$IND_PARSER_FIELDS$
} Parser;

typedef struct {
  size_t pos;
  size_t cap_len;
  int stack_size;$IND_PP_FIELD$
} ParserPosition;

typedef struct {
  size_t pos;
} ParserInputPosition;

// Set the Lua stack top, keeping the parser's shadow copy in sync. Any
// batched lua_checkstack claim beyond what survives GC stack shrinking is
// forfeited: capacity may shrink to twice the in-use size, but never below
// the runtime's minimum allocation (conservatively PGEN_STACK_FLOOR).
#define PGEN_SETTOP(parser, n) \
  do { \
    int pgen_newtop_ = (n); \
    lua_settop((parser)->L, pgen_newtop_); \
    (parser)->top = pgen_newtop_; \
    int pgen_keep_ = 2 * pgen_newtop_; \
    if (pgen_keep_ < PGEN_STACK_FLOOR) pgen_keep_ = PGEN_STACK_FLOOR; \
    if ((parser)->stack_claimed > pgen_keep_) \
      (parser)->stack_claimed = pgen_keep_; \
  } while (0)

#define REMEMBER_POSITION(parser, pp) \
  ParserPosition pp; \
  (pp).pos = (parser)->pos; \
  (pp).cap_len = (parser)->cap_len; \
  (pp).stack_size = (parser)->top;$IND_REMEMBER$

// Restore parser position
#define RESTORE_POSITION(parser, pp) \
  (parser)->pos = (pp).pos; \
  (parser)->cap_len = (pp).cap_len; \
  PGEN_SETTOP(parser, (pp).stack_size);$IND_RESTORE$

#define REMEMBER_INPUT_POSITION(parser, pp) \
  ParserInputPosition pp; \
  (pp).pos = (parser)->pos;

#define RESTORE_INPUT_POSITION(parser, pp) \
  (parser)->pos = (pp).pos;

// Records the furthest input position where a match attempt failed (only
// ever increases). Because the parser can only attempt a position it
// reached by matching everything before it, the furthest failure is the
// deepest progress into the input; parse() reports it when the overall
// parse fails without a label.
//
// Not recorded in single-character matchers (literal char, range, set):
// they fail constantly as the parser tries alternatives, and any position
// they fail at also gets tried by larger patterns (multi-char literals,
// tries, predicates, indent checks), so skipping them keeps the cost too
// small to measure without losing useful precision.
//
// Compile with -DPGEN_NO_FURTHEST to remove the tracking entirely (parse()
// then reports position 1 on ordinary failure).
#ifdef PGEN_NO_FURTHEST
#define PGEN_RECORD_FURTHEST(parser) ((void)0)
#else
#define PGEN_RECORD_FURTHEST(parser) \
  do { \
    if ((parser)->pos > (parser)->furthest_fail) \
      (parser)->furthest_fail = (parser)->pos; \
  } while (0)
#endif

// Ensure the Lua stack can hold n more values. Captures are built on the Lua
// stack, so without this a large parse tree would overflow it (undefined
// behavior). Raises a Lua error when the stack cannot grow any further
// (LUAI_MAXCSTACK).
//
// Claims are batched so most calls are a single comparison against the
// shadow top. Batch size is limited to what survives GC stack shrinking:
// PUC Lua honors lua_checkstack claims for the frame's lifetime, but
// LuaJIT's GC may shrink capacity to twice the in-use size (never below
// its minimum allocation, conservatively PGEN_STACK_FLOOR). Claims above
// released stack space are forfeited by PGEN_SETTOP.
#define PGEN_STACK_BATCH 64
#define PGEN_STACK_FLOOR 32

static void pgen_checkstack_slow(Parser *parser, int n) {
  int batch = parser->top - n;                       // survives 2x-used shrink
  int floor_batch = PGEN_STACK_FLOOR - (parser->top + n); // under shrink floor
  if (floor_batch > batch) batch = floor_batch;
  if (batch > PGEN_STACK_BATCH) batch = PGEN_STACK_BATCH;
  if (batch < 0) batch = 0;
  if (lua_checkstack(parser->L, n + batch)) {
    parser->stack_claimed = parser->top + n + batch;
  } else if (lua_checkstack(parser->L, n)) {
    // Batched request exceeded the stack limit; the exact one still fits
    parser->stack_claimed = parser->top + n;
  } else {
    luaL_error(parser->L, "pgen: Lua stack overflow while building captures");
  }
}

// Fast path: one comparison against the already-claimed capacity. n may be
// evaluated twice, so call sites must pass side-effect-free expressions.
#define pgen_checkstack(parser, n) \
  do { \
    if ((parser)->top + (n) > (parser)->stack_claimed) \
      pgen_checkstack_slow(parser, n); \
  } while (0)

static void pgen_cap_grow(Parser *parser) {
  size_t new_cap = parser->cap_cap * 2;
  PgenCap *caps = (PgenCap*)realloc(parser->caps, new_cap * sizeof(PgenCap));
  if (!caps) {
    luaL_error(parser->L, "pgen: out of memory growing capture log");
  }
  parser->caps = caps;
  parser->cap_cap = new_cap;
}

// Append one log entry. A macro so the hot path (bounds check + four
// stores) inlines into every capture site; arguments may be evaluated
// twice, so call sites must pass side-effect-free expressions.
#define pgen_cap_push(parser, k, a, s, l) \
  do { \
    if ((parser)->cap_len == (parser)->cap_cap) pgen_cap_grow(parser); \
    PgenCap *pgen_cap_ = &(parser)->caps[(parser)->cap_len++]; \
    pgen_cap_->kind = (k); \
    pgen_cap_->aux = (a); \
    pgen_cap_->start = (s); \
    pgen_cap_->len = (l); \
  } while (0)

// Advance *i past one complete log item (a single entry, or a whole
// bracketed Ct/Cg range including anything nested)
static void pgen_cap_skip(Parser *parser, size_t *i) {
  int kind = parser->caps[*i].kind;
  (*i)++;
  if (PGEN_CAP_IS_OPEN(kind)) {
    int depth = 1;
    while (depth > 0) {
      kind = parser->caps[*i].kind;
      if (PGEN_CAP_IS_OPEN(kind)) depth++;
      else if (PGEN_CAP_IS_CLOSE(kind)) depth--;
      (*i)++;
    }
  }
}

// Reduce caps[base..] to only the nth capture value (group captures don't
// count), or to a single nil when there are fewer than n values
static void pgen_cap_select(Parser *parser, size_t base, int n) {
  size_t i = base;
  int count = 0;
  while (i < parser->cap_len) {
    if (parser->caps[i].kind == PGEN_CAP_GROUP_OPEN) {
      pgen_cap_skip(parser, &i);
      continue;
    }
    size_t item_start = i;
    pgen_cap_skip(parser, &i);
    count++;
    if (count == n) {
      size_t item_len = i - item_start;
      memmove(&parser->caps[base], &parser->caps[item_start], item_len * sizeof(PgenCap));
      parser->cap_len = base + item_len;
      return;
    }
  }
  parser->cap_len = base;
  pgen_cap_push(parser, PGEN_CAP_NIL, 0, 0, 0);
}

// Match the text of the most recent visible "name" capture group at the
// current input position. Groups inside completed capture tables are not
// visible, mirroring the previous stack-based behavior where Ct consumed
// its inner captures.
static bool pgen_cap_match_back(Parser *parser, int name_idx) {
  size_t i = parser->cap_len;
  while (i > 0) {
    i--;
    int kind = parser->caps[i].kind;
    if (PGEN_CAP_IS_CLOSE(kind)) {
      size_t close = i;
      int depth = 1;
      while (depth > 0) {
        i--;
        int k2 = parser->caps[i].kind;
        if (PGEN_CAP_IS_CLOSE(k2)) depth++;
        else if (PGEN_CAP_IS_OPEN(k2)) depth--;
      }
      if (kind == PGEN_CAP_GROUP_CLOSE && parser->caps[i].aux == name_idx) {
        const char *text;
        size_t text_len;
        size_t inner = i + 1;
        if (inner == close) {
          // group captured nothing: its value is the text it matched
          text = parser->input + parser->caps[i].start;
          text_len = parser->caps[close].start - parser->caps[i].start;
        } else if (parser->caps[inner].kind == PGEN_CAP_STR) {
          text = parser->input + parser->caps[inner].start;
          text_len = parser->caps[inner].len;
        } else if (parser->caps[inner].kind == PGEN_CAP_CONST) {
          // interned constant: compare through the materialized value
          bool matched = false;
          pgen_checkstack(parser, 1);
          lua_rawgeti(parser->L, LUA_REGISTRYINDEX, parser->caps[inner].aux);
          if (lua_type(parser->L, -1) == LUA_TSTRING) {
            size_t const_len;
            const char *const_str = lua_tolstring(parser->L, -1, &const_len);
            matched = parser->pos + const_len <= parser->input_len &&
                memcmp(parser->input + parser->pos, const_str, const_len) == 0;
            if (matched) parser->pos += const_len;
          }
          lua_pop(parser->L, 1);
          return matched;
        } else {
          return false;  // group holds a non-string value
        }
        if (parser->pos + text_len <= parser->input_len &&
            memcmp(parser->input + parser->pos, text, text_len) == 0) {
          parser->pos += text_len;
          return true;
        }
        return false;
      }
    }
  }
  return false;
}

$IND_HELPERS$

#ifdef PGEN_DEBUG
static void dumpstack (lua_State *L) {
  int top=lua_gettop(L);
  for (int i=1; i <= top; i++) {
    printf("%d\t%s\t", i, luaL_typename(L,i));
    switch (lua_type(L, i)) {
      case LUA_TNUMBER:
        printf("%g\n",lua_tonumber(L,i));
        break;
      case LUA_TSTRING:
        printf("%s\n",lua_tostring(L,i));
        break;
      case LUA_TBOOLEAN:
        printf("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
        break;
      case LUA_TNIL:
        printf("%s\n", "nil");
        break;
      default:
        printf("%p\n",lua_topointer(L,i));
        break;
    }
  }
}
#endif

]], header_vars) .. generate_cg_names(cg_names) ..
    generate_const_infrastructure(const_pool or {}, cg_names) ..
    generate_cap_evaluator() ..
    generate_cmt_infrastructure(cmt_codes or {})
end

-- Generate forward declarations for all rules
function generator.generate_forward_declarations(rules, start_rule)
  local result = "// Forward declarations\n"

  for name in sorted_rules(rules, start_rule) do
    result = result .. template_code("static bool parse_$NAME$(Parser *parser);\n", {NAME = name})
  end

  return result .. "\n"
end

-- Generate functions for each rule
function generator.generate_rule_functions(rules, start_rule, const_index, cg_names)
  local result = "// Rule functions\n"
  local analyze = require("pgen.analyze")
  local cg_name_index = {}
  for i, name in ipairs(cg_names or {}) do
    cg_name_index[name] = i - 1
  end
  local context = {
    analyze = analyze,
    rules = rules,
    stateful_rules = analyze.stateful_rules(rules),
    const_index = const_index or {},
    cg_name_index = cg_name_index
  }
  for name, pattern in sorted_rules(rules, start_rule) do
    result = result .. generator.generate_rule_function(name, pattern, context)
  end

  return result
end

-- Generate a function for a specific rule
function generator.generate_rule_function(name, pattern, context)
  return template_code([[static bool parse_$NAME$(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "$NAME$", start);
#endif

  $BODY$

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "$NAME$", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "$NAME$", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

]], {
    NAME = name,
    BODY = generator.generate_pattern_code(pattern, context)
  })
end

-- Generate code for a pattern
function generator.generate_pattern_code(pattern, context)
  local t = pattern.type

  if t == types.P then -- P (literal string)
    local literal = pattern.value
    if type(literal) == "number" then
      return generator.generate_n_chars_code(literal)
    else
      return generator.generate_literal_code(literal)
    end

  elseif t == types.R then -- R (character range)
    return generator.generate_range_code(pattern.value)
  elseif t == types.S then -- S (character set)
    local set = pattern.value
    return generator.generate_set_code(set)
  elseif t == types.V then -- V (reference to another rule)
    local rule_name = pattern.value
    return generator.generate_rule_call_code(rule_name)
  elseif t == types.C then -- C (capture)
    return generator.generate_capture_code(pattern.value, context)
  elseif t == types.Ct then -- Ct (capture table)
    return generator.generate_capture_table_code(pattern.value, pattern.array_only, context)
  elseif t == types.Cp then -- Cp (capture position)
    return generator.generate_position_capture_code()
  elseif t == types.Cc then -- Cc (constant capture)
    return generator.generate_constant_capture_code(pattern.value, context)
  elseif t == types.L then -- L (lookahead)
    return generator.generate_lookahead_code(pattern.value, context)
  elseif t == types.Cg then -- Cg (capture group)
    return generator.generate_capture_group_code(pattern.value, pattern.name, context)
  elseif t == types.Cn then -- Cn (numbered capture)
    return generator.generate_numbered_capture_code(pattern.value, pattern.name, context)
  elseif t == types.Cmb then -- Cmb (capture match back)
    return generator.generate_capture_match_back_code(pattern.name, context)
  elseif t == types.Cmt then -- Cmt (match-time capture)
    return generator.generate_cmt_code(pattern.value, pattern.cmt_id, context)
  elseif t == types.Cfn then -- Cfn (transform capture)
    return generator.generate_cfn_code(pattern.value, pattern.cmt_id, context)
  elseif t == types.T then -- T (labeled failure)
    return generator.generate_labeled_failure_code(pattern.value)
  elseif t == types.Ind then -- Ind (indenter stack operation)
    return generator.generate_indenter_code(pattern)
  elseif t == "sequence" then
    return generator.generate_sequence_code(pattern, context)
  elseif t == "choice" then
    return generator.generate_choice_code(pattern[1], pattern[2], context)
  -- TODO: this is redundant, but might be able to optimize ^-1 with reduced code
  --elseif t == "optional" then
  --  return generator.generate_optional_code(pattern[1])
  elseif t == "repeat" then
    return generator.generate_repeat_code(pattern[1], pattern[2], context)
  elseif t == "negate" then
    return generator.generate_negate_code(pattern[1], context)
  elseif t == "literal_trie" then
    return generator.generate_trie_code(pattern.trie, pattern.strings)
  else
    error("Unknown pattern type: " .. tostring(t))
  end
end

-- Generate code for a literal string match
function generator.generate_literal_code(literal)
  -- Optimization for single character literals - use direct comparison instead of memcmp
  if #literal == 1 then
    return template_code([[{// Match single character $ESCAPED_LITERAL$
  if (parser->pos < parser->input_len &&
      parser->input[parser->pos] == $CHAR_CODE$) {
    parser->pos++;
  } else {
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Expected character `" $ERROR_LITERAL$ "` at position %zu", parser->pos);
#endif
    parser->success = false;
  }
}]], {
      ESCAPED_LITERAL = escape_string(literal),
      ERROR_LITERAL = escape_c_literal(escape_c_literal(literal, "")),
      CHAR_CODE = string.byte(literal)
    })
  end

  return template_code([[{// Match literal $ESCAPED_LITERAL$
if (parser->pos + $LITERAL_LEN$ <= parser->input_len &&
    memcmp(parser->input + parser->pos, $LITERAL$, $LITERAL_LEN$) == 0) {
  parser->pos += $LITERAL_LEN$;
} else {
#ifdef PGEN_ERRORS
  sprintf(parser->error_message, "Expected `" $LITERAL$ "` at position %zu", parser->pos);
#endif
  parser->success = false;
  PGEN_RECORD_FURTHEST(parser);
}
}]], {
    ESCAPED_LITERAL = escape_string(literal),
    LITERAL = escape_c_literal(literal),
    LITERAL_LEN = #literal
  })
end

-- Generate code for matching exactly n characters
function generator.generate_n_chars_code(n)
  return template_code([[{// Match any $N$ characters
  if (parser->pos + $N$ <= parser->input_len) {
    parser->pos += $N$;
  } else {
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Expected at least $N$ more characters at position %zu", parser->pos);
#endif
    parser->success = false;
    PGEN_RECORD_FURTHEST(parser);
  }
}]], {
    N = n
  })
end

-- Generate code for multiple character ranges
-- ranges: array of two-character strings representing low and upper bounds characters
function generator.generate_range_code(ranges)
  local conditions = {}
  local error_ranges = {}

  for i, range in ipairs(ranges) do
    local range_left, range_right = range:match("^(.)(.)")
    assert(range_left, "range must have two characters: " .. range)
    assert(range_left <= range_right, "range must be in ascending order: " .. range)

    table.insert(conditions, template_code("(parser->input[parser->pos] >= $LOW$ && parser->input[parser->pos] <= $HIGH$)", {
      LOW = string.byte(range_left),
      HIGH = string.byte(range_right)
    }))

    table.insert(error_ranges, template_code([[$LEFT$ " - " $RIGHT$]], {
      LEFT = escape_c_literal(range_left),
      RIGHT = escape_c_literal(range_right)
    }))
  end

  local condition_str = table.concat(conditions, " || ")
  local error_ranges_str = table.concat(error_ranges, [[", "]])

  return template_code([[{// Match character range: $RANGES$
  if (parser->pos < parser->input_len &&
      ($CONDITION$)) {
    parser->pos++;
  } else {
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Expected character in ranges [" $ERROR_RANGES$ "] at position %zu", parser->pos);
#endif
    parser->success = false;
  }
}]], {
    RANGES = escape_string(table.concat(ranges, ",")),
    CONDITION = condition_str,
    ERROR_RANGES = error_ranges_str
  })
end

-- Generate code for a character set match
function generator.generate_set_code(set)
  -- Generate cases for the switch
  local cases = {}
  for i = 1, #set do
    local c = set:sub(i, i)
    table.insert(cases, template_code("    case $BYTE$: /* $CHAR$ */", {
      BYTE = tostring(string.byte(c)),
      CHAR = escape_c_literal(c)
    }))
  end

  return template_code([[{// Match character set $SET$
  if (parser->pos < parser->input_len) {
    switch (parser->input[parser->pos]) {
$CASES$
      parser->pos++;
      break;
    default:
#ifdef PGEN_ERRORS
      sprintf(parser->error_message, "Expected one of " $SET_LITERAL$ " at position %zu", parser->pos);
#endif
      parser->success = false;
    }
  } else {
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Expected one of " $SET_LITERAL$ " at position %zu but reached end of input", parser->pos);
#endif
    parser->success = false;
  }
}]], {
    SET = escape_string(set),
    CASES = table.concat(cases, "\n"),
    SET_LITERAL = escape_c_literal(escape_c_literal(set))
  })
end

-- Generate code for a rule call
function generator.generate_rule_call_code(rule_name)
  return template_code([[parse_$RULE$(parser);]], {
  RULE = rule_name
})
end

-- Generate code for a sequence
function generator.generate_sequence_code(patterns, context)
  if #patterns == 1 then
    -- TODO: throw an error here? shouldn't happen
    return generator.generate_pattern_code(patterns[1], context)
  end

  local remember, restore = position_operations(patterns, context)

  local function step(idx)
    local rest = patterns[idx + 1] and template_code([[
if (parser->success) {
  $PATTERN$
  $RESET$
}]], {
      PATTERN = step(idx + 1),
      RESET = idx == 1 and "if (!parser->success) { " .. restore .. " }" or ""
    })

    return table.concat({
      generator.generate_pattern_code(patterns[idx], context),
      rest
    },"\n")
  end

  -- Generate the initial position storage and first pattern
  return template_code([[{// Sequence with $N$ patterns
$REMEMBER$

$SEQUENCE$
}]], {
    N = #patterns,
    REMEMBER = remember,
    SEQUENCE = step(1)
  })
end

-- Generate code for a choice
function generator.generate_choice_code(a, b, context)
  return template_code([[{ // Choice
  $A$

  // Only try alternative if ordinary failure (not labeled failure from T())
  if (!parser->success && !parser->throw_label) {
    parser->success = true;
    $B$
  }
}]], {
  A = generator.generate_pattern_code(a, context),
  B = generator.generate_pattern_code(b, context)
})
end

-- Generate code for number of repetitions
-- if n is zero or positive, at least n repetitions
-- if n is negative, at most n repetitions
function generator.generate_repeat_code(a, n, context)
  if n < 0 then
    local at_most = -n

  return template_code([[{ // At most $N$ repetitions
  size_t rep_count = 0;

  while(rep_count < $N$) {
    size_t before_pos = parser->pos;

    {
      $BODY$
    }

    if (!parser->success || before_pos == parser->pos) {
      // Break on failure or zero-width match
      // Only recover from ordinary failure, not labeled failure from T()
      if (!parser->throw_label) {
        parser->success = true;
      }
      break;
    }

    rep_count += 1;
  }
}]], {
      N = at_most,
      BODY = generator.generate_pattern_code(a, context)
    })
  end

  if n == 0 then
    return template_code([[{ // Zero or more repetitions
  while(true) {
    $BODY$
    if (!parser->success) {
      break;
    }
  }
  // Only recover from ordinary failure, not labeled failure from T()
  if (!parser->throw_label) {
    parser->success = true;
  }
}]], {
      BODY = generator.generate_pattern_code(a, context)
    })
  end


  local remember, restore = position_operations(a, context)

  return template_code([[{ // At least $N$ repetitions
  $REMEMBER$
  size_t rep_count = 0;

  while(true) {
    $BODY$

    if (!parser->success) {
      break;
    }

    rep_count += 1;
  }

  // Don't recover if labeled failure was thrown
  if (parser->throw_label) {
    // Keep failure state, propagate labeled failure
  } else if (rep_count >= $N$) {
    parser->success = true;
  } else {
    $RESTORE$
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Expected $N$ repetitions at position %zu", parser->pos);
#endif
  }
}]], {
    N = n,
    REMEMBER = remember,
    RESTORE = restore,
    BODY = generator.generate_pattern_code(a, context)
  })
end

-- Generate code for a negated pattern
function generator.generate_negate_code(a, context)
  local remember, restore = position_operations(a, context)

  return template_code([[{// Negate (only match if pattern fails)
  $REMEMBER$

  $BODY$

  if (parser->success) {
    // Pattern matched, so negate fails
    $RESTORE$
    parser->success = false;
    PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Negated pattern unexpectedly matched at position %zu", pos.pos);
#endif
  } else {
    // Pattern failed, so negate succeeds
    parser->success = true;
    // Swallow labeled failures inside predicates (LPegLabel behavior)
    if (parser->throw_label) {
      parser->throw_label = NULL;
      parser->throw_pos = 0;
    }
    $RESTORE$ // Restore original position (technically not necessary since failed pattern should make no changes to position)
  }
}]], {
    REMEMBER = remember,
    RESTORE = restore,
    BODY = generator.generate_pattern_code(a, context)
  })
end

-- Generate code for a capture
function generator.generate_capture_code(body, context)
  return template_code([[{ // Capture
  size_t start_pos = parser->pos;
  $BODY$

  if (parser->success) {
    pgen_cap_push(parser, PGEN_CAP_STR, 0, start_pos, parser->pos - start_pos);
  }
}]], {
    BODY = generator.generate_pattern_code(body, context)
  })
end

-- Generate code for a capture table (Ct)
-- Emits open/close brackets in the capture log; the table itself (array
-- part plus named Cg fields) is built by the evaluator after the parse.
-- The array_only flag is no longer needed: materialization happens once
-- per surviving table, so there is no per-attempt cost to optimize away.
function generator.generate_capture_table_code(body, array_only, context)
  return template_code([[{ // Capture Table
  size_t ct_cap_start = parser->cap_len;
  pgen_cap_push(parser, PGEN_CAP_TBL_OPEN, 0, 0, 0);
  $BODY$

  if (parser->success) {
    pgen_cap_push(parser, PGEN_CAP_TBL_CLOSE, 0, 0, 0);
  } else {
    parser->cap_len = ct_cap_start;
  }
}]], {
    BODY = generator.generate_pattern_code(body, context)
  })
end

-- Generate code for a position capture (Cp)
function generator.generate_position_capture_code()
  return template_code([[{ // Position Capture
  pgen_cap_push(parser, PGEN_CAP_POS, 0, parser->pos, 0);
}]], {})
end

-- Generate code for a constant capture (Cc)
-- Each value becomes one log entry referencing the interned constant;
-- matching never constructs the values themselves
function generator.generate_constant_capture_code(values, context)
  local push_code = ""
  local const_index = context and context.const_index or {}

  -- Note: [[ ]] strings strip their leading newline, so each segment gets
  -- an explicit "\n" prefix. Without it segments join on one line, and the
  -- interned-constant comment would swallow the following statement.
  for i=1, values.count do
    local value = values[i]
    local t = type(value)
    if t == "nil" then
      push_code = push_code .. "\n  pgen_cap_push(parser, PGEN_CAP_NIL, 0, 0, 0);"
    elseif t == "string" or t == "number" or t == "boolean" then
      local idx = const_index[value]
      if not idx then
        error("Cc value was not interned during collection: " .. tostring(value))
      end
      local comment = t == "string" and
        " // " .. escape_c_literal(value):gsub("%*/", "* /") or
        " // " .. tostring(value)
      push_code = push_code .. "\n" .. template_code(
        [[  pgen_cap_push(parser, PGEN_CAP_CONST, __const_refs[$IDX$], 0, 0);]],
        {IDX = idx}) .. comment
    else
      error("Unsupported constant capture type: " .. t)
    end
  end

  return template_code([[{ // Constant Capture
  // A constant capture matches the empty string and produces all given values$PUSH_CODE$
}]], {
    PUSH_CODE = push_code
  })
end

-- Generate code for a lookahead pattern (L)
function generator.generate_lookahead_code(body, context)
  local remember, restore = position_operations(body, context)

  return template_code([[{// Lookahead (match without consuming input)
  $REMEMBER$

  $BODY$

  if (parser->success) {
    // Pattern matched, but we don't consume any input
    $RESTORE$
  }
}]], {
    REMEMBER = remember,
    RESTORE = restore,
    BODY = generator.generate_pattern_code(body, context)
  })
end

-- Generate code for a capture group (Cg)
-- Emits open/close brackets in the capture log carrying the name index and
-- the input span; the evaluator resolves the group's value (first inner
-- capture, or the matched text when it contains no captures)
function generator.generate_capture_group_code(body, name, context)
  return template_code([[{ // Capture Group "$NAME$"
  size_t cg_cap_start = parser->cap_len;
  pgen_cap_push(parser, PGEN_CAP_GROUP_OPEN, $NAME_IDX$, parser->pos, 0);
  $BODY$

  if (parser->success) {
    pgen_cap_push(parser, PGEN_CAP_GROUP_CLOSE, $NAME_IDX$, parser->pos, 0);
  } else {
    parser->cap_len = cg_cap_start;
  }
}]], {
    BODY = generator.generate_pattern_code(body, context),
    NAME = name,
    NAME_IDX = assert(context.cg_name_index[name], "Cg name not collected: " .. name)
  })
end

-- Generate code for a numbered capture (Cn)
-- Selects the nth capture from inner pattern, or no captures if n=0
-- If n > capture count, returns nil
function generator.generate_numbered_capture_code(body, n, context)
  if n == 0 then
    return template_code([[{ // Numbered Capture (discard all)
  size_t cn_cap_start = parser->cap_len;
  $BODY$

  if (parser->success) {
    parser->cap_len = cn_cap_start;
  }
}]], {
      BODY = generator.generate_pattern_code(body, context)
    })
  end

  return template_code([[{ // Numbered Capture (select $N$)
  size_t cn_cap_start = parser->cap_len;
  $BODY$

  if (parser->success) {
    pgen_cap_select(parser, cn_cap_start, $N$);
  }
}]], {
    BODY = generator.generate_pattern_code(body, context),
    N = n
  })
end

-- Generate code for capture match back (Cmb)
-- Searches the capture log backward for the named group and matches its
-- text against the input at the current position
function generator.generate_capture_match_back_code(name, context)
  return template_code([[{ // Capture Match Back "$NAME$"
  parser->success = pgen_cap_match_back(parser, $NAME_IDX$);
  if (!parser->success) {
    PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Capture match back '$NAME$' failed at position %zu", parser->pos);
#endif
  }
}]], {
    NAME = name,
    NAME_IDX = assert(context.cg_name_index[name], "Cmb name not collected: " .. name)
  })
end

-- Generate code for match-time capture (Cmt)
-- Calls a Lua callback during parsing with (subject, position, ...captures)
function generator.generate_cmt_code(inner_pattern, cmt_id, context)
  return template_code([[{ // Match-time capture (Cmt id=$ID$)
  size_t cmt_cap_base = parser->cap_len;
  int cmt_top_base = parser->top;
  size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
  size_t cmt_trail_index = parser->trail_len;
#endif

  $INNER_PATTERN_CODE$

  if (parser->success) {
    pgen_run_cmt(parser, __cmt_refs[$ID$], cmt_start_pos, cmt_cap_base, cmt_top_base);

#ifdef PGEN_HAS_IND
    // Callback rejected the match: undo indenter operations performed by the
    // inner pattern (an inner failure rewinds itself)
    if (!parser->success) {
      pgen_ind_trail_rewind(parser, cmt_trail_index);
    }
#endif
  }
}]], {
    ID = cmt_id,
    INNER_PATTERN_CODE = generator.generate_pattern_code(inner_pattern, context)
  })
end

-- Generate code for a transform capture (Cfn)
-- Emits open/close brackets in the capture log carrying the callback's
-- registry ref and the matched span; the callback runs during
-- materialization, so backtracked-over transforms are never called
function generator.generate_cfn_code(body, cmt_id, context)
  return template_code([[{ // Transform Capture (Cfn id=$ID$)
  size_t fn_cap_start = parser->cap_len;
  pgen_cap_push(parser, PGEN_CAP_FN_OPEN, __cmt_refs[$ID$], parser->pos, 0);
  $BODY$

  if (parser->success) {
    pgen_cap_push(parser, PGEN_CAP_FN_CLOSE, 0, parser->pos, 0);
  } else {
    parser->cap_len = fn_cap_start;
  }
}]], {
    ID = cmt_id,
    BODY = generator.generate_pattern_code(body, context)
  })
end

-- Generate code for an indenter stack operation (Ind)
-- All operations are transactional: pushes/pops are recorded on the trail
-- and undone when the parser backtracks past them
function generator.generate_indenter_code(pattern)
  local op = pattern.op
  local sid = pattern.stack_id

  if sid == nil then
    error("Ind node has no stack_id assigned; compile the grammar through pgen.compile/generator.generate")
  end

  local vars = {
    SID = sid,
    TW = pattern.indenter.tab_width or 4
  }

  if op == "check" then
    return template_code([[{ // Indenter check (stack $SID$): consume whitespace, width must equal top
  size_t ind_end;
  int ind_width = pgen_ind_measure(parser, &ind_end, $TW$);
  PgenIndStack *ind_s = &parser->ind_stacks[$SID$];
  if (ind_s->size > 0 && ind_s->items[ind_s->size - 1] == ind_width) {
    parser->pos = ind_end;
  } else {
    parser->success = false;
    PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Indent width %d does not match current level at position %zu", ind_width, parser->pos);
#endif
  }
}]], vars)
  elseif op == "advance" then
    return template_code([[{ // Indenter advance (stack $SID$): push width if deeper than top, consume nothing
  size_t ind_end;
  int ind_width = pgen_ind_measure(parser, &ind_end, $TW$);
  (void)ind_end;
  PgenIndStack *ind_s = &parser->ind_stacks[$SID$];
  if (ind_s->size > 0 && ind_width > ind_s->items[ind_s->size - 1]) {
    pgen_ind_push(parser, $SID$, ind_width);
  } else {
    parser->success = false;
    PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Indent width %d does not advance current level at position %zu", ind_width, parser->pos);
#endif
  }
}]], vars)
  elseif op == "push" then
    return template_code([[{ // Indenter push (stack $SID$): consume whitespace, push measured width
  size_t ind_end;
  int ind_width = pgen_ind_measure(parser, &ind_end, $TW$);
  pgen_ind_push(parser, $SID$, ind_width);
  parser->pos = ind_end;
}]], vars)
  elseif op == "prevent" then
    return template_code([[{ // Indenter prevent (stack $SID$): push sentinel so nested advance fails
  pgen_ind_push(parser, $SID$, PGEN_IND_PREVENT_SENTINEL);
}]], vars)
  elseif op == "pop" then
    return template_code([[{ // Indenter pop (stack $SID$)
  if (!pgen_ind_pop(parser, $SID$)) {
    parser->success = false;
    PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Indenter stack $SID$ is empty at position %zu", parser->pos);
#endif
  }
}]], vars)
  elseif op == "cpush" then
    vars.VALUE = pattern.value
    return template_code([[{ // Indenter cpush (stack $SID$): push constant $VALUE$
  pgen_ind_push(parser, $SID$, $VALUE$);
}]], vars)
  elseif op == "ctop" then
    local cmp_ops = {
      eq = "==", ne = "!=", lt = "<", le = "<=", gt = ">", ge = ">="
    }
    vars.VALUE = pattern.value
    vars.CMP = pattern.cmp
    vars.CMP_OP = cmp_ops[pattern.cmp] or error("Unknown ctop comparison: " .. tostring(pattern.cmp))
    return template_code([[{ // Indenter ctop (stack $SID$): top $CMP$ $VALUE$
  PgenIndStack *ind_s = &parser->ind_stacks[$SID$];
  if (!(ind_s->size > 0 && ind_s->items[ind_s->size - 1] $CMP_OP$ $VALUE$)) {
    parser->success = false;
    PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Indenter stack $SID$ top failed $CMP$ $VALUE$ check at position %zu", parser->pos);
#endif
  }
}]], vars)
  else
    error("Unknown indenter operation: " .. tostring(op))
  end
end

-- Generate code for labeled failure throw
function generator.generate_labeled_failure_code(label)
  local escaped_label = escape_c_literal(label)

  return template_code([[
{// Throw labeled failure: $LABEL$
  parser->success = false;
  parser->throw_label = $ESCAPED_LABEL$;
  parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
  sprintf(parser->error_message, $ESCAPED_LABEL$ " at position %zu", parser->pos + 1);
#endif
}]], {
    LABEL = label,
    ESCAPED_LABEL = escaped_label
  })
end

-- Generate core C parser functions (_init, _free, _parse)
function generator.generate_c_core_functions(parser_name, start_rule, indenters)
  indenters = indenters or {}

  local ind_null = ""
  local ind_init = ""
  local ind_free = ""

  if #indenters > 0 then
    local initials = {}
    for _, ind in ipairs(indenters) do
      table.insert(initials, tostring(ind.initial))
    end

    ind_null = [[

  parser->trail = NULL;
  parser->trail_len = 0;
  parser->trail_cap = 0;
  for (int i = 0; i < PGEN_IND_STACK_COUNT; i++) {
    parser->ind_stacks[i].items = NULL;
  }]]

    ind_init = template_code([[


  // Initialize indenter stacks (each starts holding its initial value)
  static const int pgen_ind_initials[PGEN_IND_STACK_COUNT] = { $INITIALS$ };
  for (int i = 0; i < PGEN_IND_STACK_COUNT; i++) {
    parser->ind_stacks[i].items = (int*)malloc(8 * sizeof(int));
    if (!parser->ind_stacks[i].items) {
      luaL_error(L, "pgen: out of memory initializing parser");
    }
    parser->ind_stacks[i].cap = 8;
    parser->ind_stacks[i].size = 1;
    parser->ind_stacks[i].items[0] = pgen_ind_initials[i];
  }
]], {INITIALS = table.concat(initials, ", ")})

    ind_free = [[

     for (int i = 0; i < PGEN_IND_STACK_COUNT; i++) {
       free(parser->ind_stacks[i].items);
       parser->ind_stacks[i].items = NULL;
     }
     free(parser->trail);
     parser->trail = NULL;]]
  end

  return template_code([[
#define PGEN_PARSER_MT "pgen.$PARSER_NAME$"

// Initialize a parser anchored in a Lua userdata (left on the stack). Its
// metatable's __gc frees the owned allocations, so a Lua error unwinding
// out of a parse (transform/Cmt callbacks, recursion depth, out of memory)
// cannot leak them.
static Parser* $PARSER_NAME$_init(const char *input, lua_State *L) {
  Parser *parser = (Parser*)lua_newuserdata(L, sizeof(Parser));

  // Null the owned pointers before attaching the metatable so __gc is
  // safe even if a later allocation fails mid-init
  parser->caps = NULL;$IND_NULL$
  luaL_getmetatable(L, PGEN_PARSER_MT);
  lua_setmetatable(L, -2);

  parser->input = input;
  parser->input_len = strlen(input);
  parser->pos = 0;
  parser->depth = 0;
  parser->success = true;
  parser->error_message[0] = '\0';
  parser->throw_label = NULL;
  parser->throw_pos = 0;
  parser->furthest_fail = 0;
  parser->top = lua_gettop(L);
  parser->stack_claimed = parser->top;
  parser->L = L;

  parser->caps = (PgenCap*)malloc(64 * sizeof(PgenCap));
  if (!parser->caps) {
    luaL_error(L, "pgen: out of memory initializing parser");
  }
  parser->cap_len = 0;
  parser->cap_cap = 64;$IND_INIT$
  return parser;
}


// Free the parser's owned allocations. Idempotent: called eagerly on
// normal completion and again from __gc, which also covers error unwinds
static void $PARSER_NAME$_free(Parser *parser) {
  if (parser) {$IND_FREE$
     free(parser->caps);
     parser->caps = NULL;
  }
}
]], {
    PARSER_NAME = parser_name,
    START_RULE = start_rule,
    IND_NULL = ind_null,
    IND_INIT = ind_init,
    IND_FREE = ind_free
  })
end

-- Generate C code for the Lua module interface
function generator.generate_lua_module_code(parser_name, start_rule, cmt_codes, has_consts)
  cmt_codes = cmt_codes or {}
  local cmt_init = #cmt_codes > 0 and "__cmt_init(L);" or ""
  local const_init = has_consts and "__const_init(L);" or ""

  return template_code([[
// --- Lua Module Interface ---

// __gc for the parser userdata: frees whatever the eager free didn't
static int l_$PARSER_NAME$_gc(lua_State *L) {
  $PARSER_NAME$_free((Parser*)lua_touserdata(L, 1));
  return 0;
}

// Lua wrapper function
static int l_$PARSER_NAME$_parse(lua_State *L) {
  // Check type and get the input string
  if (!lua_isstring(L, 1)) {
    return luaL_error(L, "Expected string argument for parsing");
  }
  const char *input = lua_tostring(L, 1);
  if (!input) {
      // Should not happen if lua_isstring passed, but good practice
      return luaL_error(L, "Failed to get string argument");
  }

  // Initialize the parser (a userdata anchored on the stack; see _init)
  Parser *parser = $PARSER_NAME$_init(input, L);

  int initial_stack_size = lua_gettop(parser->L);

  parse_$START_RULE$(parser);

  int final_stack_size = lua_gettop(parser->L);
  assert(parser->top == final_stack_size && "Shadow stack top out of sync.");

  // Return nil and error info on failure
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size && "Unexpected stack size change on parse failure.");
    assert(parser->cap_len == 0 && "Capture log not empty on parse failure.");
    lua_pushnil(L);
    if (parser->throw_label) {
      // Labeled failure: return nil, label, position
      lua_pushstring(L, parser->throw_label);
      lua_pushinteger(L, parser->throw_pos + 1);  // 1-indexed for Lua
      $PARSER_NAME$_free(parser);
      return 3;
    } else {
      // Ordinary failure: return nil, message (PGEN_ERRORS builds only) and
      // the furthest input position a match attempt failed at (1-indexed)
#ifdef PGEN_ERRORS
      lua_pushstring(L, parser->error_message);
#else
      lua_pushnil(L);
#endif
      lua_pushinteger(L, parser->furthest_fail + 1);
      $PARSER_NAME$_free(parser);
      return 3;
    }
  }

  // Materialize the capture log into return values. Named groups produce
  // no top-level values (they only matter inside Ct).
  int cmt_slots = parser->top - initial_stack_size;  // lingering Cmt values
  int result_count = 0;
  size_t cap_i = 0;
  while (cap_i < parser->cap_len) {
    if (parser->caps[cap_i].kind == PGEN_CAP_GROUP_OPEN) {
      pgen_cap_skip(parser, &cap_i);
    } else {
      result_count += pgen_cap_eval(parser, &cap_i);
    }
  }

  // Drop the lingering Cmt value slots sitting beneath the results
  for (int i = 0; i < cmt_slots; i++) {
    lua_remove(L, initial_stack_size + 1);
  }

  if (result_count > 0) {
    $PARSER_NAME$_free(parser);
    return result_count;
  }

  // Success case with no captures
  lua_pushinteger(L, parser->pos + 1);
  $PARSER_NAME$_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg $PARSER_NAME$_module[] = {
  {"parse", l_$PARSER_NAME$_parse}, // Expose l_parsername_parse as "parse" in Lua
  {NULL, NULL} // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
  // Lua 5.2+ uses luaL_setfuncs
  int luaopen_$PARSER_NAME$(lua_State *L) {
    if (luaL_newmetatable(L, PGEN_PARSER_MT)) {
      lua_pushcfunction(L, l_$PARSER_NAME$_gc);
      lua_setfield(L, -2, "__gc");
    }
    lua_pop(L, 1);
    $CONST_INIT$
    $CMT_INIT$
    luaL_newlib(L, $PARSER_NAME$_module); // Creates table and registers functions
    return 1;
  }
#else
  // Lua 5.1 uses luaL_register. Register into a fresh table rather than a
  // named global: a name would be shared through package.loaded, so loading
  // two parsers compiled with the same parser_name in one process would
  // silently overwrite the first module's parse function.
  int luaopen_$PARSER_NAME$(lua_State *L) {
    if (luaL_newmetatable(L, PGEN_PARSER_MT)) {
      lua_pushcfunction(L, l_$PARSER_NAME$_gc);
      lua_setfield(L, -2, "__gc");
    }
    lua_pop(L, 1);
    $CONST_INIT$
    $CMT_INIT$
    lua_newtable(L);
    luaL_register(L, NULL, $PARSER_NAME$_module);
    return 1;
  }
#endif
]], {
  PARSER_NAME = parser_name,
  START_RULE = start_rule,
  CMT_INIT = cmt_init,
  CONST_INIT = const_init
})
end

-- Generate the final combined parser main C code
function generator.generate_parser_main(parser_name, start_rule, cmt_codes, indenters, has_consts)
  -- core C functions
  local c_core_code = generator.generate_c_core_functions(parser_name, start_rule, indenters)
  -- Lua module interface
  local lua_module_code = generator.generate_lua_module_code(parser_name, start_rule, cmt_codes, has_consts)

  return c_core_code .. "\n" .. lua_module_code
end

-- Generate C code for trie-based literal matching
function generator.generate_trie_code(trie, strings)
  local code = generator.generate_trie_node_code(trie, 0)
  local display_strings = {}
  for i, str in ipairs(strings) do
    display_strings[i] = escape_string(str)
  end

  return template_code([[{// Trie match for: $STRINGS$
REMEMBER_POSITION(parser, trie_start);
size_t last_terminal_pos = 0;
int has_terminal = 0;
$TRIE_CODE$
if (!parser->success) {
  RESTORE_POSITION(parser, trie_start);
}
}]], {
    STRINGS = table.concat(display_strings, ", "),
    TRIE_CODE = code
  })
end

-- Recursively generate switch statements for trie traversal
function generator.generate_trie_node_code(node, depth)
  local preamble = ""
  if node.is_terminal then
    preamble = [[if (!has_terminal || parser->pos > last_terminal_pos) {
  last_terminal_pos = parser->pos;
  has_terminal = 1;
}]]
  end

  -- Collect and sort cases for deterministic output
  local chars = {}
  for char in pairs(node.children) do
    table.insert(chars, char)
  end
  table.sort(chars)

  local cases = {}
  for _, char in ipairs(chars) do
    local child = node.children[char]
    local case_body

    if child.is_terminal and not next(child.children) then
      -- Leaf node: just advance and succeed
      case_body = "parser->pos++;"
    elseif child.is_terminal then
      -- Terminal with more children: try to continue, but partial match is OK
      case_body = template_code([[parser->pos++;
$CHILD_CODE$
if (!parser->success) {
  // Partial match is valid: "$WORD$"
  parser->success = true;
}]], {
        CHILD_CODE = generator.generate_trie_node_code(child, depth + 1),
        WORD = child.word
      })
    else
      -- Non-terminal: must continue matching
      case_body = template_code([[parser->pos++;
$CHILD_CODE$]], {
        CHILD_CODE = generator.generate_trie_node_code(child, depth + 1)
      })
    end

    table.insert(cases, template_code([[  case $BYTE$: // $CHAR$
    $BODY$
    break;]], {
      BYTE = string.byte(char),
      CHAR = escape_c_literal(char),
      BODY = case_body
    }))
  end

  return template_code([[$PREAMBLE$
if (parser->pos < parser->input_len) {
  switch (parser->input[parser->pos]) {
$CASES$
  default:
    parser->success = false;
    if (has_terminal) {
      parser->pos = last_terminal_pos;
      parser->success = true;
    } else {
      PGEN_RECORD_FURTHEST(parser);
    }
  }
} else {
  parser->success = false;
  if (has_terminal) {
    parser->pos = last_terminal_pos;
    parser->success = true;
  } else {
    PGEN_RECORD_FURTHEST(parser);
  }
}]], {
    PREAMBLE = preamble,
    CASES = table.concat(cases, "\n")
  })
end

return generator

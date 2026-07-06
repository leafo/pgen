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

-- Collect all Cmt nodes from a grammar, assigning each a unique ID
-- Returns array of {id, code} for unique codes, and a new grammar with cmt_id fields set
-- Identical code strings share the same ID (deduplication)
local function collect_cmt_codes(grammar)
  local visitor = require("pgen.visitor")
  local pgen = require("pgen")
  local codes = {}           -- Array of {id, code} for unique codes only
  local code_to_id = {}      -- Map: code string -> id (for deduplication)
  local next_id = 0

  local new_grammar = visitor.visit_grammar(grammar, function(node, replace)
    -- Only replace Cmt nodes that don't already have an ID assigned
    if node.type == types.Cmt and node.cmt_id == nil then
      local code = node.code
      local id = code_to_id[code]

      if id == nil then
        -- First time seeing this code, assign new ID
        id = next_id
        code_to_id[code] = id
        table.insert(codes, {id = id, code = code})
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

  -- Collect all Cg (capture group) names for sentinel generation
  local cg_names = collect_cg_names(transformed_grammar)
  for _, name in ipairs(cg_names) do
    assert_valid_c_identifier(name)
  end

  -- Generate the C code
  local c_chunks = {
    generator.generate_parser_header(parser_name, cg_names, cmt_codes, indenters),
    generator.generate_forward_declarations(rules, start_rule),
    generator.generate_rule_functions(rules, start_rule),
    generator.generate_parser_main(parser_name, start_rule, cmt_codes, indenters),
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
    talbe.insert(c_chunks, 1, "#define PGEN_DEBUG 1")
  end

  if options.pgen_errors then
    table.insert(c_chunks, 1, "#define PGEN_ERRORS 1")
  end

  return table.concat(c_chunks, "\n")
end

-- Generate sentinel declarations for Cg names
-- Always generates is_cg_sentinel function (returns false if no sentinels)
local function generate_cg_sentinels(cg_names)
  local lines = {}

  if #cg_names > 0 then
    table.insert(lines, "// Named capture group sentinels")

    -- Generate sentinel declarations
    for _, name in ipairs(cg_names) do
      table.insert(lines, template_code(
        "static const char __cg_sentinel_$NAME$[] = $NAME_STR$;",
        {NAME = name, NAME_STR = escape_c_literal(name)}
      ))
    end

    -- Generate registry array
    table.insert(lines, "")
    table.insert(lines, "// Registry of all known sentinels (for validation)")
    table.insert(lines, "static const void* __cg_sentinel_registry[] = {")
    for _, name in ipairs(cg_names) do
      table.insert(lines, template_code("  (void*)__cg_sentinel_$NAME$,", {NAME = name}))
    end
    table.insert(lines, "  NULL  // terminator")
    table.insert(lines, "};")

    -- Generate validation function that checks the registry
    table.insert(lines, "")
    table.insert(lines, [[// Check if a pointer is one of our Cg sentinels
static bool is_cg_sentinel(void* ptr) {
  for (int i = 0; __cg_sentinel_registry[i] != NULL; i++) {
    if (ptr == __cg_sentinel_registry[i]) return true;
  }
  return false;
}]])
  else
    -- No Cg names - generate a stub function that always returns false
    table.insert(lines, [[// No Cg sentinels defined - stub function
static bool is_cg_sentinel(void* ptr) {
  (void)ptr; // unused
  return false;
}]])
  end

  return table.concat(lines, "\n") .. "\n"
end

-- Generate Cmt (match-time capture) infrastructure
-- Returns C code for: static code strings, ref array, and init function
local function generate_cmt_infrastructure(cmt_codes)
  if #cmt_codes == 0 then
    return ""
  end

  local lines = {}
  table.insert(lines, "// Match-time capture (Cmt) infrastructure")
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
  table.insert(lines, [[// Initialize Cmt callbacks by loading their Lua code
static void __cmt_init(lua_State *L) {]])

  for _, cmt in ipairs(cmt_codes) do
    table.insert(lines, template_code([[  if (luaL_loadstring(L, __cmt_code_$ID$) != 0) {
    luaL_error(L, "Failed to load Cmt callback $ID$: %s", lua_tostring(L, -1));
  }
  __cmt_refs[$ID$] = luaL_ref(L, LUA_REGISTRYINDEX);]], {ID = cmt.id}))
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
function generator.generate_parser_header(parser_name, cg_names, cmt_codes, indenters)
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

$IND_TYPES$typedef struct {
  const char *input;
  size_t input_len;
  size_t pos;
  bool success;
  char error_message[256];
  const char *throw_label;  // Label from T() or NULL for ordinary failure
  size_t throw_pos;         // Position where T() was thrown
  size_t depth;
  lua_State *L;$IND_PARSER_FIELDS$
} Parser;

typedef struct {
  size_t pos;
  int stack_size;$IND_PP_FIELD$
} ParserPosition;

#define REMEMBER_POSITION(parser, pp) \
  ParserPosition pp; \
  (pp).pos = (parser)->pos; \
  (pp).stack_size = lua_gettop((parser)->L);$IND_REMEMBER$

// Restore parser position
#define RESTORE_POSITION(parser, pp) \
  (parser)->pos = (pp).pos; \
  lua_settop((parser)->L, (pp).stack_size);$IND_RESTORE$

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

]], header_vars) .. generate_cg_sentinels(cg_names) .. generate_cmt_infrastructure(cmt_codes or {})
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
function generator.generate_rule_functions(rules, start_rule)
  local result = "// Rule functions\n"

  for name, pattern in sorted_rules(rules, start_rule) do
    result = result .. generator.generate_rule_function(name, pattern)
  end

  return result
end

-- Generate a function for a specific rule
function generator.generate_rule_function(name, pattern)
  return template_code([[static bool parse_$NAME$(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
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
  parser->depth -= 1;
#endif

  return parser->success;
}

]], {
    NAME = name,
    BODY = generator.generate_pattern_code(pattern)
  })
end

-- Generate code for a pattern
function generator.generate_pattern_code(pattern)
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
    return generator.generate_capture_code(pattern.value)
  elseif t == types.Ct then -- Ct (capture table)
    return generator.generate_capture_table_code(pattern.value, pattern.array_only)
  elseif t == types.Cp then -- Cp (capture position)
    return generator.generate_position_capture_code()
  elseif t == types.Cc then -- Cc (constant capture)
    return generator.generate_constant_capture_code(pattern.value)
  elseif t == types.L then -- L (lookahead)
    return generator.generate_lookahead_code(pattern.value)
  elseif t == types.Cg then -- Cg (capture group)
    return generator.generate_capture_group_code(pattern.value, pattern.name)
  elseif t == types.Cn then -- Cn (numbered capture)
    return generator.generate_numbered_capture_code(pattern.value, pattern.name)
  elseif t == types.Cmb then -- Cmb (capture match back)
    return generator.generate_capture_match_back_code(pattern.name)
  elseif t == types.Cmt then -- Cmt (match-time capture)
    return generator.generate_cmt_code(pattern.value, pattern.cmt_id)
  elseif t == types.T then -- T (labeled failure)
    return generator.generate_labeled_failure_code(pattern.value)
  elseif t == types.Ind then -- Ind (indenter stack operation)
    return generator.generate_indenter_code(pattern)
  elseif t == "sequence" then
    return generator.generate_sequence_code(unpack(pattern))
  elseif t == "choice" then
    return generator.generate_choice_code(pattern[1], pattern[2])
  -- TODO: this is redundant, but might be able to optimize ^-1 with reduced code
  --elseif t == "optional" then
  --  return generator.generate_optional_code(pattern[1])
  elseif t == "repeat" then
    return generator.generate_repeat_code(pattern[1], pattern[2])
  elseif t == "negate" then
    return generator.generate_negate_code(pattern[1])
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
function generator.generate_sequence_code(...)
  local patterns = {...}

  if #patterns == 1 then
    -- TODO: throw an error here? shouldn't happen
    return generator.generate_pattern_code(patterns[1])
  end

  function step(idx, current, ...)
    local rest = ... and template_code([[
if (parser->success) {
  $PATTERN$
  $RESET$
}]], {
      PATTERN = step(idx + 1, ...),
      RESET = idx == 1 and "if (!parser->success) { RESTORE_POSITION(parser, pos); }" or ""
    })

    return table.concat({
      generator.generate_pattern_code(current),
      rest
    },"\n")
  end

  -- Generate the initial position storage and first pattern
  return template_code([[{// Sequence with $N$ patterns
REMEMBER_POSITION(parser, pos);

$SEQUENCE$
}]], {
    N = #patterns,
    SEQUENCE = step(1, unpack(patterns))
  })
end

-- Generate code for a choice
function generator.generate_choice_code(a, b)
  return template_code([[{ // Choice
  $A$

  // Only try alternative if ordinary failure (not labeled failure from T())
  if (!parser->success && !parser->throw_label) {
    parser->success = true;
    $B$
  }
}]], {
  A = generator.generate_pattern_code(a),
  B = generator.generate_pattern_code(b)
})
end

-- Generate code for number of repetitions
-- if n is zero or positive, at least n repetitions
-- if n is negative, at most n repetitions
function generator.generate_repeat_code(a, n)
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
      BODY = generator.generate_pattern_code(a)
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
      BODY = generator.generate_pattern_code(a)
    })
  end


  return template_code([[{ // At least $N$ repetitions
  REMEMBER_POSITION(parser, pos);
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
    RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
    sprintf(parser->error_message, "Expected $N$ repetitions at position %zu", parser->pos);
#endif
  }
}]], {
    N = n,
    BODY = generator.generate_pattern_code(a)
  })
end

-- Generate code for a negated pattern
function generator.generate_negate_code(a)
  return template_code([[{// Negate (only match if pattern fails)
  REMEMBER_POSITION(parser, pos);

  $BODY$

  if (parser->success) {
    // Pattern matched, so negate fails
    RESTORE_POSITION(parser, pos);
    parser->success = false;
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
    RESTORE_POSITION(parser, pos); // Restore original position (technically not necessary since failed pattern should make no changes to position)
  }
}]], {
    BODY = generator.generate_pattern_code(a)
  })
end

-- Generate code for a capture
function generator.generate_capture_code(body)
  return template_code([[{ // Capture
  size_t start_pos = parser->pos;
  $BODY$

  if (parser->success) {
    size_t capture_length = parser->pos - start_pos;
    // TODO: ensure stack has enough space for push
    lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
  }
}]], {
    BODY = generator.generate_pattern_code(body)
  })
end

-- Generate code for a capture table (Ct)
-- Handles both regular captures (array part) and named captures via Cg (hash part)
function generator.generate_capture_table_code(body, array_only)
  if array_only then
    return template_code([[{ // Capture Table (array-only)
  int initial_stack_size = lua_gettop(parser->L);
  $BODY$

  if (parser->success) {
    int new_stack_size = lua_gettop(parser->L);
    int items_start = initial_stack_size + 1;
    int array_count = new_stack_size - initial_stack_size;

    lua_createtable(parser->L, array_count, 0);
    int table_idx = lua_gettop(parser->L);

    int array_idx = 1;
    for (int i = items_start; i < table_idx; i++) {
      lua_pushvalue(parser->L, i);
      lua_rawseti(parser->L, table_idx, array_idx++);
    }

    // Remove all items except table, move table to correct position
    if (items_start <= new_stack_size) {
      lua_replace(parser->L, items_start);
      lua_settop(parser->L, items_start);
    }
  }
}]], {
      BODY = generator.generate_pattern_code(body)
    })
  end

  return template_code([[{ // Capture Table
  int initial_stack_size = lua_gettop(parser->L);
  $BODY$

  if (parser->success) {
    int new_stack_size = lua_gettop(parser->L);
    int items_start = initial_stack_size + 1;

    // Count array items and named items separately
    // Named captures are sentinel (light userdata) + value pairs
    int array_count = 0;
    int named_count = 0;
    for (int i = items_start; i <= new_stack_size; i++) {
      if (lua_islightuserdata(parser->L, i) &&
          is_cg_sentinel(lua_touserdata(parser->L, i))) {
        named_count++;
        i++; // skip the value that follows the sentinel
      } else {
        array_count++;
      }
    }

    lua_createtable(parser->L, array_count, named_count);
    int table_idx = lua_gettop(parser->L);

    int array_idx = 1;
    for (int i = items_start; i < table_idx; i++) {
      if (lua_islightuserdata(parser->L, i)) {
        void* ptr = lua_touserdata(parser->L, i);
        if (is_cg_sentinel(ptr)) {
          // Named capture: sentinel at i, value at i+1
          const char* name = (const char*)ptr;
          lua_pushstring(parser->L, name);
          lua_pushvalue(parser->L, i + 1);
          lua_rawset(parser->L, table_idx);
          i++; // skip value
          continue;
        }
      }
      // Regular capture (including non-sentinel light userdata): add to array part
      lua_pushvalue(parser->L, i);
      lua_rawseti(parser->L, table_idx, array_idx++);
    }

    // Remove all items except table, move table to correct position
    // Only needed if there were items to remove (items_start <= new_stack_size)
    if (items_start <= new_stack_size) {
      lua_replace(parser->L, items_start);
      lua_settop(parser->L, items_start);
    }
  }
}]], {
    BODY = generator.generate_pattern_code(body)
  })
end

-- Generate code for a position capture (Cp)
function generator.generate_position_capture_code()
  return template_code([[{ // Position Capture
  // Push current position + 1 (Lua uses 1-based indexing)
  lua_pushinteger(parser->L, parser->pos + 1);
}]], {})
end

-- Generate code for a constant capture (Cc)
function generator.generate_constant_capture_code(values)
  local push_code = ""

  for i=1, values.count do
    local value = values[i]
    local t = type(value)
    if t == "string" then
      push_code = push_code .. template_code([[
  lua_pushlstring(parser->L, $VALUE$, $LENGTH$);]], {
        VALUE = escape_c_literal(value),
        LENGTH = #value
      })
    elseif t == "number" then
      push_code = push_code .. template_code([[
  lua_pushnumber(parser->L, $VALUE$);]], {
        VALUE = value
      })
    elseif t == "boolean" then
      push_code = push_code .. template_code([[
  lua_pushboolean(parser->L, $VALUE$);]], {
        VALUE = value and "1" or "0"
      })
    elseif t == "nil" then
      push_code = push_code .. [[
  lua_pushnil(parser->L);]]
    else
      error("Unsupported constant capture type: " .. t)
    end
  end

  return template_code([[{ // Constant Capture
  // A constant capture matches the empty string and produces all given values
$PUSH_CODE$
}]], {
    PUSH_CODE = push_code
  })
end

-- Generate code for a lookahead pattern (L)
function generator.generate_lookahead_code(body)
  return template_code([[{// Lookahead (match without consuming input)
  REMEMBER_POSITION(parser, pos);

  $BODY$

  if (parser->success) {
    // Pattern matched, but we don't consume any input
    RESTORE_POSITION(parser, pos);
  }
}]], {
    BODY = generator.generate_pattern_code(body)
  })
end

-- Generate code for a capture group (Cg)
-- Pushes two values to the stack: a sentinel (light userdata) and the captured value
-- If inner pattern produces captures, uses the first capture; otherwise captures matched text
function generator.generate_capture_group_code(body, name)
  return template_code([[{ // Capture Group "$NAME$"
  int cg_stack_start = lua_gettop(parser->L);
  size_t start_pos = parser->pos;
  $BODY$

  if (parser->success) {
    int cg_stack_end = lua_gettop(parser->L);
    int captures_produced = cg_stack_end - cg_stack_start;

    if (captures_produced > 0) {
      // Inner pattern produced captures - use the first one
      // Push sentinel (identifies this as named capture "$NAME$")
      lua_pushlightuserdata(parser->L, (void*)__cg_sentinel_$NAME$);
      // Move sentinel before the first capture
      lua_insert(parser->L, cg_stack_start + 1);
      // Now stack is: sentinel, first_capture, [other_captures...]
      // Remove any extra captures (keep only sentinel + first capture)
      lua_settop(parser->L, cg_stack_start + 2);
    } else {
      // No captures - capture the matched text span
      size_t capture_len = parser->pos - start_pos;
      // Push sentinel (identifies this as named capture "$NAME$")
      lua_pushlightuserdata(parser->L, (void*)__cg_sentinel_$NAME$);
      // Push captured value
      lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
    }
  }
}]], {
    BODY = generator.generate_pattern_code(body),
    NAME = name
  })
end

-- Generate code for a numbered capture (Cn)
-- Selects the nth capture from inner pattern, or no captures if n=0
-- If n > capture count, returns nil
function generator.generate_numbered_capture_code(body, n)
  if n == 0 then
    return template_code([[{ // Numbered Capture (discard all)
  int cn_stack_start = lua_gettop(parser->L);
  $BODY$

  if (parser->success) {
    lua_settop(parser->L, cn_stack_start);
  }
}]], {
      BODY = generator.generate_pattern_code(body)
    })
  end

  return template_code([[{ // Numbered Capture (select $N$)
  int cn_stack_start = lua_gettop(parser->L);
  $BODY$

  if (parser->success) {
    int final_stack = lua_gettop(parser->L);

    // Find the nth non-sentinel capture (Cg sentinel+value pairs don't count)
    int count = 0;
    int target_idx = 0;
    for (int i = cn_stack_start + 1; i <= final_stack; i++) {
      if (lua_islightuserdata(parser->L, i) && is_cg_sentinel(lua_touserdata(parser->L, i))) {
        i++; // skip sentinel's value too
        continue;
      }
      count++;
      if (count == $N$) {
        target_idx = i;
        break;
      }
    }

    if (target_idx > 0) {
      lua_pushvalue(parser->L, target_idx);
      lua_replace(parser->L, cn_stack_start + 1);
      lua_settop(parser->L, cn_stack_start + 1);
    } else {
      lua_settop(parser->L, cn_stack_start);
      lua_pushnil(parser->L);
    }
  }
}]], {
    BODY = generator.generate_pattern_code(body),
    N = n
  })
end

-- Generate code for capture match back (Cmb)
-- Searches backward through stack for named capture sentinel and matches its string value
function generator.generate_capture_match_back_code(name)
  return template_code([[{ // Capture Match Back "$NAME$"
  parser->success = false;

  // Search backward through stack for sentinel "$NAME$"
  for (int i = lua_gettop(parser->L); i >= 1; i--) {
    if (lua_islightuserdata(parser->L, i)) {
      void* ptr = lua_touserdata(parser->L, i);
      if (ptr == (void*)__cg_sentinel_$NAME$) {
        // Found sentinel - value is at i+1 (use only if it's already a string)
        if (i + 1 <= lua_gettop(parser->L) && lua_type(parser->L, i + 1) == LUA_TSTRING) {
          size_t match_len;
          const char* match_str = lua_tolstring(parser->L, i + 1, &match_len);
          // Try to match at current position
          if (parser->pos + match_len <= parser->input_len &&
              memcmp(parser->input + parser->pos, match_str, match_len) == 0) {
            parser->pos += match_len;
            parser->success = true;
          }
        }
        break;  // Found sentinel, stop searching regardless of match result
      }
    }
  }
#ifdef PGEN_ERRORS
  if (!parser->success) {
    sprintf(parser->error_message, "Capture match back '$NAME$' failed at position %zu", parser->pos);
  }
#endif
}]], {
    NAME = name
  })
end

-- Generate code for match-time capture (Cmt)
-- Calls a Lua callback during parsing with (subject, position, ...captures)
function generator.generate_cmt_code(inner_pattern, cmt_id)
  return template_code([[{ // Match-time capture (Cmt id=$ID$)
  int cmt_stack_base = lua_gettop(parser->L);
  size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
  size_t cmt_trail_index = parser->trail_len;
#endif

  $INNER_PATTERN_CODE$

  if (parser->success) {
    size_t pos_after_inner = parser->pos;
    int captures_count = lua_gettop(parser->L) - cmt_stack_base;

    // Get the callback function from registry
    lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cmt_refs[$ID$]);

    // Push arguments: subject, position (after inner pattern)
    lua_pushlstring(parser->L, parser->input, parser->input_len);
    lua_pushinteger(parser->L, (lua_Integer)(pos_after_inner + 1));  // 1-based

    // Copy captures as additional arguments
    for (int i = 0; i < captures_count; i++) {
      lua_pushvalue(parser->L, cmt_stack_base + 1 + i);
    }

    // Remove original captures from stack before calling
    // Stack: [captures...][func][subject][pos][captures_copy...]
    for (int i = 0; i < captures_count; i++) {
      lua_remove(parser->L, cmt_stack_base + 1);
    }
    // Stack now: [func][subject][pos][captures...]

    // Call function: 2 + captures_count args, LUA_MULTRET returns
    // lua_call propagates errors (aborts parse on Lua error)
    lua_call(parser->L, 2 + captures_count, LUA_MULTRET);

    int returns_count = lua_gettop(parser->L) - cmt_stack_base;

    if (returns_count == 0) {
      // No return value = match fails
      parser->success = false;
      parser->pos = cmt_start_pos;
    } else {
      int first_type = lua_type(parser->L, cmt_stack_base + 1);

      if (first_type == LUA_TNUMBER) {
        // Number = new position (1-based from Lua)
        lua_Integer new_pos = lua_tointeger(parser->L, cmt_stack_base + 1);
        new_pos--;  // Convert to 0-based
        // Per lpeg: must be in range [pos_after_inner, input_len]
        if (new_pos >= (lua_Integer)pos_after_inner && new_pos <= (lua_Integer)parser->input_len) {
          parser->pos = (size_t)new_pos;
          parser->success = true;
        } else {
          parser->success = false;
          parser->pos = cmt_start_pos;
        }
      } else if (first_type == LUA_TBOOLEAN && lua_toboolean(parser->L, cmt_stack_base + 1)) {
        // true = succeed without consuming (position stays at pos_after_inner)
        parser->success = true;
      } else {
        // false, nil, or other = fail
        parser->success = false;
        parser->pos = cmt_start_pos;
      }
    }

    // Handle captures: remove first return value, keep extras as captures
    if (parser->success && returns_count > 1) {
      lua_remove(parser->L, cmt_stack_base + 1);  // Remove first return value
      // Remaining values are the new captures
    } else {
      lua_settop(parser->L, cmt_stack_base);  // Clear all returns
    }

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
    INNER_PATTERN_CODE = generator.generate_pattern_code(inner_pattern)
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

  local ind_init = ""
  local ind_free = ""

  if #indenters > 0 then
    local initials = {}
    for _, ind in ipairs(indenters) do
      table.insert(initials, tostring(ind.initial))
    end

    ind_init = template_code([[


  // Initialize indenter stacks (each seeded with its initial value)
  static const int pgen_ind_initials[PGEN_IND_STACK_COUNT] = { $INITIALS$ };
  parser->trail = NULL;
  parser->trail_len = 0;
  parser->trail_cap = 0;
  for (int i = 0; i < PGEN_IND_STACK_COUNT; i++) {
    parser->ind_stacks[i].items = (int*)malloc(8 * sizeof(int));
    if (!parser->ind_stacks[i].items) {
      for (int j = 0; j < i; j++) {
        free(parser->ind_stacks[j].items);
      }
      free(parser);
      return NULL;
    }
    parser->ind_stacks[i].cap = 8;
    parser->ind_stacks[i].size = 1;
    parser->ind_stacks[i].items[0] = pgen_ind_initials[i];
  }
]], {INITIALS = table.concat(initials, ", ")})

    ind_free = [[

     for (int i = 0; i < PGEN_IND_STACK_COUNT; i++) {
       free(parser->ind_stacks[i].items);
     }
     free(parser->trail);]]
  end

  return template_code([[
// Initialize parser
static Parser* $PARSER_NAME$_init(const char *input, lua_State *L) {
  Parser *parser = (Parser*)malloc(sizeof(Parser));
  if (!parser) {
    // Handle allocation failure if necessary, though often parser might exit
    return NULL;
  }
  parser->input = input;
  parser->input_len = strlen(input);
  parser->pos = 0;
  parser->depth = 0;
  parser->success = true;
  parser->error_message[0] = '\0';
  parser->throw_label = NULL;
  parser->throw_pos = 0;
  parser->L = L;$IND_INIT$
  return parser;
}


// Free parser
static void $PARSER_NAME$_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {$IND_FREE$
     free(parser);
  }
}
]], {
    PARSER_NAME = parser_name,
    START_RULE = start_rule,
    IND_INIT = ind_init,
    IND_FREE = ind_free
  })
end

-- Generate C code for the Lua module interface
function generator.generate_lua_module_code(parser_name, start_rule, cmt_codes)
  cmt_codes = cmt_codes or {}
  local cmt_init = #cmt_codes > 0 and "__cmt_init(L);" or ""

  return template_code([[
// --- Lua Module Interface ---

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

  // Initialize the parser directly
  Parser *parser = $PARSER_NAME$_init(input, L);
  if (!parser) {
     lua_pushnil(L);
     lua_pushstring(L, "Parser initialization failed (memory allocation?)");
     return 2;
  }

  int initial_stack_size = lua_gettop(parser->L);

  parse_$START_RULE$(parser);

  int final_stack_size = lua_gettop(parser->L);

  // Return nil and error info on failure
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size && "Unexpected stack size change on parse failure.");
    lua_pushnil(L);
    if (parser->throw_label) {
      // Labeled failure: return nil, label, position
      lua_pushstring(L, parser->throw_label);
      lua_pushinteger(L, parser->throw_pos + 1);  // 1-indexed for Lua
      $PARSER_NAME$_free(parser);
      return 3;
    } else {
      // Ordinary failure: return nil, error_message
      lua_pushstring(L, parser->error_message);
      $PARSER_NAME$_free(parser);
      return 2;
    }
  }

  // Strip Cg sentinel+value pairs from stack (they only matter inside Ct)
  if (final_stack_size > initial_stack_size) {
    int read_idx = initial_stack_size + 1;
    int write_idx = initial_stack_size + 1;
    while (read_idx <= final_stack_size) {
      if (lua_islightuserdata(L, read_idx) && is_cg_sentinel(lua_touserdata(L, read_idx))) {
        // Skip sentinel and its value
        read_idx += 2;
      } else {
        if (read_idx != write_idx) {
          lua_pushvalue(L, read_idx);
          lua_replace(L, write_idx);
        }
        read_idx++;
        write_idx++;
      }
    }
    lua_settop(L, write_idx - 1);
    final_stack_size = lua_gettop(L);
  }

  // If stack size has changed, use new items as return values
  if (final_stack_size > initial_stack_size) {
    $PARSER_NAME$_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
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
    $CMT_INIT$
    luaL_newlib(L, $PARSER_NAME$_module); // Creates table and registers functions
    return 1;
  }
#else
  // Lua 5.1 uses luaL_register
  int luaopen_$PARSER_NAME$(lua_State *L) {
    $CMT_INIT$
    luaL_register(L, "$PARSER_NAME$", $PARSER_NAME$_module); // Registers functions in global table (or package table)
    return 1;
  }
#endif
]], {
  PARSER_NAME = parser_name,
  START_RULE = start_rule,
  CMT_INIT = cmt_init
})
end

-- Generate the final combined parser main C code
function generator.generate_parser_main(parser_name, start_rule, cmt_codes, indenters)
  -- core C functions
  local c_core_code = generator.generate_c_core_functions(parser_name, start_rule, indenters)
  -- Lua module interface
  local lua_module_code = generator.generate_lua_module_code(parser_name, start_rule, cmt_codes)

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
    }
  }
} else {
  parser->success = false;
  if (has_terminal) {
    parser->pos = last_terminal_pos;
    parser->success = true;
  }
}]], {
    PREAMBLE = preamble,
    CASES = table.concat(cases, "\n")
  })
end

return generator

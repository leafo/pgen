local generator = {}

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

-- Collect all Cg names from a grammar
local function collect_cg_names(grammar)
  local pgen = require("pgen")
  local names = {}
  pgen.visit_grammar(grammar, function(node)
    if node.type == 10 then -- Cg
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

  local rules = {}
  local start_rule = nil

  -- Extract rules from grammar
  for name, pattern in pairs(grammar) do
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
  local cg_names = collect_cg_names(grammar)

  -- Generate the C code
  local c_chunks = {
    generator.generate_parser_header(parser_name, cg_names),
    generator.generate_forward_declarations(rules, start_rule),
    generator.generate_rule_functions(rules, start_rule),
    generator.generate_parser_main(parser_name, start_rule),
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

-- Generate parser header
function generator.generate_parser_header(parser_name, cg_names)
  cg_names = cg_names or {}
  return template_code([[#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <assert.h>

// $PARSER_NAME$ - generated parser

typedef struct {
  const char *input;
  size_t input_len;
  size_t pos;
  bool success;
  char error_message[256];
  size_t depth;
  lua_State *L;
} Parser;

typedef struct {
  size_t pos;
  int stack_size;
} ParserPosition;

#define REMEMBER_POSITION(parser, pp) \
  ParserPosition pp; \
  (pp).pos = (parser)->pos; \
  (pp).stack_size = lua_gettop((parser)->L);

// Restore parser position
#define RESTORE_POSITION(parser, pp) \
  (parser)->pos = (pp).pos; \
  lua_settop((parser)->L, (pp).stack_size);


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

]], {PARSER_NAME = parser_name}) .. generate_cg_sentinels(cg_names)
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

  if t == 1 then -- P (literal string)
    local literal = pattern.value
    if type(literal) == "number" then
      return generator.generate_n_chars_code(literal)
    else
      return generator.generate_literal_code(literal)
    end

  elseif t == 2 then -- R (character range)
    return generator.generate_range_code(pattern.value)
  elseif t == 3 then -- S (character set)
    local set = pattern.value
    return generator.generate_set_code(set)
  elseif t == 4 then -- V (reference to another rule)
    local rule_name = pattern.value
    return generator.generate_rule_call_code(rule_name)
  elseif t == 5 then -- C (capture)
    return generator.generate_capture_code(pattern.value)
  elseif t == 6 then -- Ct (capture table)
    return generator.generate_capture_table_code(pattern.value, pattern.array_only)
  elseif t == 7 then -- Cp (capture position)
    return generator.generate_position_capture_code()
  elseif t == 8 then -- Cc (constant capture)
    return generator.generate_constant_capture_code(pattern.value)
  elseif t == 9 then -- L (lookahead)
    return generator.generate_lookahead_code(pattern.value)
  elseif t == 10 then -- Cg (capture group)
    return generator.generate_capture_group_code(pattern.value, pattern.name)
  elseif t == 11 then -- Cn (numbered capture)
    return generator.generate_numbered_capture_code(pattern.value, pattern.name)
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

  if (!parser->success) {
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
      parser->success = true;
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
  parser->success = true;
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

  if (rep_count >= $N$) {
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
    int captures_produced = lua_gettop(parser->L) - cn_stack_start;

    if ($N$ <= captures_produced) {
      lua_pushvalue(parser->L, cn_stack_start + $N$);
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

-- Assuming 'generator' is a table used to organize functions
generator = generator or {}

-- Generate core C parser functions (_init, _free, _parse)
function generator.generate_c_core_functions(parser_name, start_rule)
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
  parser->L = L;
  return parser;
}


// Free parser
static void $PARSER_NAME$_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
     free(parser);
  }
}
]], {
    PARSER_NAME = parser_name,
    START_RULE = start_rule
  })
end

-- Generate C code for the Lua module interface
function generator.generate_lua_module_code(parser_name, start_rule)
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

  // Return nil and error message on failure, true on success
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size && "Unexpected stack size change on parse failure.");
    lua_pushnil(L);
    lua_pushstring(L, parser->error_message);
    $PARSER_NAME$_free(parser);
    return 2; // Return nil and error message
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
    luaL_newlib(L, $PARSER_NAME$_module); // Creates table and registers functions
    return 1;
  }
#else
  // Lua 5.1 uses luaL_register
  int luaopen_$PARSER_NAME$(lua_State *L) {
    luaL_register(L, "$PARSER_NAME$", $PARSER_NAME$_module); // Registers functions in global table (or package table)
    return 1;
  }
#endif
]], {
  PARSER_NAME = parser_name,
  START_RULE = start_rule
})
end

-- Generate the final combined parser main C code
function generator.generate_parser_main(parser_name, start_rule)
  -- core C functions
  local c_core_code = generator.generate_c_core_functions(parser_name, start_rule)
  -- Lua module interface
  local lua_module_code = generator.generate_lua_module_code(parser_name, start_rule)

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

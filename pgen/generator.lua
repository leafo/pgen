local generator = {}

-- human readable version of the string that can be embeded into comments
local function escape_string(str)
  return require("cjson").encode(str)
end

local function escape_c_literal(str)
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
  return '"' .. escaped .. '"'
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


-- Compile a grammar definition to C code
function generator.generate(grammar, parser_name, output_file)
  local c_code = generator.create_c_code(grammar, parser_name)

  -- Write C code to file
  local file = io.open(output_file, "w")
  if not file then
    return nil, "Could not open output file: " .. output_file
  end

  file:write(c_code)
  file:close()

  return output_file
end

-- Convert grammar to C code
function generator.create_c_code(grammar, parser_name, options)
  options = options or {}

  local rules = {}
  local start_rule = nil

  -- Extract rules from grammar
  for name, pattern in pairs(grammar) do
    if not start_rule then
      start_rule = name
    end
    rules[name] = pattern
  end

  if not start_rule then
    error("Grammar must contain at least one rule")
  end

  -- Generate the C code
  local c_code = generator.generate_parser_header(parser_name)
  c_code = c_code .. generator.generate_forward_declarations(rules)
  c_code = c_code .. generator.generate_rule_functions(rules)
  c_code = c_code .. generator.generate_parser_main(parser_name, start_rule)

  -- Add compilation instructions as a comment
  c_code = c_code .. template_code([[/*
To compile as a Lua module:
gcc -shared -o $PARSER_NAME$.so -fPIC $PARSER_NAME$.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local $PARSER_NAME$ = require "$PARSER_NAME$"
local result = $PARSER_NAME$.parse("your input string")
*/
]], {PARSER_NAME = parser_name})

  return c_code
end

-- Generate parser header
function generator.generate_parser_header(parser_name)
  return template_code([[#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// $PARSER_NAME$ - generated parser

typedef struct {
  const char *input;
  size_t input_len;
  size_t pos;
  bool success;
  char error_message[256];
} Parser;

]], {PARSER_NAME = parser_name})
end

-- Generate forward declarations for all rules
function generator.generate_forward_declarations(rules)
  local result = "// Forward declarations\n"

  for name, _ in pairs(rules) do
    result = result .. template_code("static bool parse_$NAME$(Parser *parser);\n", {NAME = name})
  end

  return result .. "\n"
end

-- Generate functions for each rule
function generator.generate_rule_functions(rules)
  local result = "// Rule functions\n"

  for name, pattern in pairs(rules) do
    result = result .. generator.generate_rule_function(name, pattern)
  end

  return result
end

-- Generate a function for a specific rule
function generator.generate_rule_function(name, pattern)
  return template_code([[static bool parse_$NAME$(Parser *parser) {
  size_t start_pos = parser->pos;

  $BODY$

  return true;
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
    return generator.generate_literal_code(literal)
  elseif t == 2 then -- R (character range)
    local range = pattern.value
    return generator.generate_range_code(range[1], range[2])
  elseif t == 3 then -- S (character set)
    local set = pattern.value
    return generator.generate_set_code(set)
  elseif t == 4 then -- V (reference to another rule)
    local rule_name = pattern.value
    return generator.generate_rule_call_code(rule_name)
  elseif t == "sequence" then
    return generator.generate_sequence_code(pattern[1], pattern[2])
  elseif t == "choice" then
    return generator.generate_choice_code(pattern[1], pattern[2])
  elseif t == "optional" then
    return generator.generate_optional_code(pattern[1])
  elseif t == "star" then
    return generator.generate_star_code(pattern[1])
  elseif t == "repeat" then
    return generator.generate_repeat_code(pattern[1], pattern[2])
  else
    error("Unknown pattern type: " .. tostring(t))
  end
end

-- Generate code for a literal string match
function generator.generate_literal_code(literal)
  return template_code([[// Match literal $ESCAPED_LITERAL$
if (parser->pos + $LITERAL_LEN$ <= parser->input_len &&
    memcmp(parser->input + parser->pos, $LITERAL$, $LITERAL_LEN$) == 0) {
  parser->pos += $LITERAL_LEN$;
} else {
  parser->pos = start_pos;
  sprintf(parser->error_message, "Expected `" $LITERAL$ "` at position %zu", parser->pos);
  parser->success = false;
  return false;
}]], {
    ESCAPED_LITERAL = escape_string(literal),
    LITERAL = escape_c_literal(literal),
    LITERAL_LEN = #literal
  })
end

-- Generate code for a character range match
function generator.generate_range_code(start, stop)
  return template_code([[// Match character range $START$ - $STOP$
if (parser->pos < parser->input_len &&
    parser->input[parser->pos] >= $START_BYTE$ && parser->input[parser->pos] <= $STOP_BYTE$) {
  parser->pos++;
} else {
  parser->pos = start_pos;
  sprintf(parser->error_message, "Expected character in range " $START_LIT$ " - " $STOP_LIT$ " at position %zu", parser->pos);
  parser->success = false;
  return false;
}]], {
    START = escape_string(start),
    STOP = escape_string(stop),
    START_BYTE = string.byte(start),
    STOP_BYTE = string.byte(stop),
    START_LIT = escape_c_literal(start),
    STOP_LIT = escape_c_literal(stop)
  })
end

-- Generate code for a character set match
function generator.generate_set_code(set)
  local cond = {}
  for i = 1, #set do
    local c = set:sub(i, i)
    table.insert(cond, template_code("parser->input[parser->pos] == $BYTE$", {
      BYTE = tostring(string.byte(c))
    }))
  end

  return template_code([[// Match character set $SET$
if (parser->pos < parser->input_len &&
    ($CONDITIONS$)) {
  parser->pos++;
} else {
  parser->pos = start_pos;
  sprintf(parser->error_message, "Expected one of " $SET_LITERAL$ " at position %zu", parser->pos);
  parser->success = false;
  return false;
}]], {
    SET = escape_string(set),
    CONDITIONS = table.concat(cond, " || "),
    SET_LITERAL = escape_c_literal(escape_c_literal(set))
  })
end

-- Generate code for a rule call
function generator.generate_rule_call_code(rule_name)
  return template_code([[// Call rule $RULE$
if (!parse_$RULE$(parser)) {
  return false;
}]], {
  RULE = rule_name
})
end

-- Generate code for a sequence
function generator.generate_sequence_code(a, b)
  return template_code([[// Sequence
$A$

$B$]], {
    A = generator.generate_pattern_code(a),
    B = generator.generate_pattern_code(b)
  })
end

-- Generate code for a choice
function generator.generate_choice_code(a, b)
  return template_code([[// Choice
{
  size_t choice_pos = parser->pos;
  bool choice_result = true;

  // Try first alternative
  $A$

  if (!choice_result) {
    // Restore position and try second alternative
    parser->pos = choice_pos;
    parser->success = true;

    $B$

    if (!parser->success) {
      parser->pos = start_pos;
      return false;
    }
  }
}]], {
  A = generator.generate_pattern_code(a),
  B = generator.generate_pattern_code(b)
})
end

-- Generate code for an optional pattern
function generator.generate_optional_code(a)
  return template_code([[// Optional
{
  size_t opt_pos = parser->pos;
  bool opt_success = parser->success;

  // Try to match but don't fail if it doesn't match
  $BODY$

  if (!parser->success) {
    // Restore position and continue
    parser->pos = opt_pos;
    parser->success = opt_success;
  }
}]], {
  BODY = generator.generate_pattern_code(a)
})
end

-- Generate code for zero or more repetitions
function generator.generate_star_code(a)
  return template_code([[// Zero or more repetitions
while (parser->success && parser->pos < parser->input_len) {
  size_t repeat_pos = parser->pos;

  // Try to match pattern
  $BODY$

  if (!parser->success || repeat_pos == parser->pos) {
    // Restore position and exit loop on failure or no progress
    parser->pos = repeat_pos;
    parser->success = true;
    break;
  }
}]], {
  BODY = generator.generate_pattern_code(a)
})
end

-- Generate code for exact number of repetitions
function generator.generate_repeat_code(a, n)
  return template_code([[// Exactly $N$ repetitions
for (int i = 0; i < $N$; i++) {
  $BODY$

  if (!parser->success) {
    parser->pos = start_pos;
    sprintf(parser->error_message, "Expected $N$ repetitions at position %zu", parser->pos);
    return false;
  }
}]], {
  N = n,
  BODY = generator.generate_pattern_code(a)
})
end


-- Assuming 'generator' is a table used to organize functions
generator = generator or {}

-- Generate core C parser functions (_init, _free, _parse)
function generator.generate_c_core_functions(parser_name, start_rule)
  return template_code([[
// Initialize parser
static Parser* $PARSER_NAME$_init(const char *input) {
  Parser *parser = (Parser*)malloc(sizeof(Parser));
  if (!parser) {
    // Handle allocation failure if necessary, though often parser might exit
    return NULL;
  }
  parser->input = input;
  parser->input_len = strlen(input);
  parser->pos = 0;
  parser->success = true;
  parser->error_message[0] = '\0';
  return parser;
}

// Free parser
static void $PARSER_NAME$_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
     free(parser);
  }
}

// Parse input using the generated start rule function
static bool $PARSER_NAME$_parse(const char *input) {
  Parser *parser = $PARSER_NAME$_init(input);
  if (!parser) {
     fprintf(stderr, "Parser initialization failed (memory allocation?).\n");
     return false; // Indicate failure if init failed
  }

  bool result = parse_$START_RULE$(parser); // Call the specific start rule parser

  // Check if entire input was consumed after a successful rule parse
  if (result && parser->pos < parser->input_len) {
    parser->success = false;
    // Use snprintf for safety against buffer overflows
    snprintf(parser->error_message, sizeof(parser->error_message),
             "Unexpected input at position %zu", parser->pos);
    result = false;
  }

  // Report error if parsing failed at any point
  if (!result && parser->success == false) { // Only print if we set an error
    fprintf(stderr, "Parse error: %s\n", parser->error_message);
  }

  // It's crucial to free the parser even if parsing failed
  $PARSER_NAME$_free(parser);
  return result;
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
  Parser *parser = $PARSER_NAME$_init(input);
  if (!parser) {
     lua_pushnil(L);
     lua_pushstring(L, "Parser initialization failed (memory allocation?)");
     return 2;
  }

  // Call the start rule parser function
  bool result = parse_$START_RULE$(parser);

  // Check if entire input was consumed
  if (result && parser->pos < parser->input_len) {
    parser->success = false;
    snprintf(parser->error_message, sizeof(parser->error_message),
             "Unexpected input at position %zu", parser->pos);
    result = false;
  }

  // Return nil and error message on failure, true on success
  if (!result) {
    lua_pushnil(L);
    lua_pushstring(L, parser->error_message);
    $PARSER_NAME$_free(parser);
    return 2; // Return nil and error message
  }

  // Success case
  lua_pushboolean(L, true);
  $PARSER_NAME$_free(parser);
  return 1; // Return just true
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

return generator

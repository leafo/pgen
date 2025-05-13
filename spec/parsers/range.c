#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// range - generated parser

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

#define REMEMBER_POSITION(parser, pos)                                         \
  ParserPosition pos;                                                          \
  (pos).pos = (parser)->pos;                                                   \
  (pos).stack_size = lua_gettop((parser)->L);

// Restore parser position
#define RESTORE_POSITION(parser, pos)                                          \
  (parser)->pos = (pos).pos;                                                   \
  lua_settop((parser)->L, (pos).stack_size);

#ifdef PGEN_DEBUG
static void dumpstack(lua_State *L) {
  int top = lua_gettop(L);
  for (int i = 1; i <= top; i++) {
    printf("%d\t%s\t", i, luaL_typename(L, i));
    switch (lua_type(L, i)) {
    case LUA_TNUMBER:
      printf("%g\n", lua_tonumber(L, i));
      break;
    case LUA_TSTRING:
      printf("%s\n", lua_tostring(L, i));
      break;
    case LUA_TBOOLEAN:
      printf("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
      break;
    case LUA_TNIL:
      printf("%s\n", "nil");
      break;
    default:
      printf("%p\n", lua_topointer(L, i));
      break;
    }
  }
}
#endif

// Forward declarations
static bool parse_1(Parser *parser);
static bool parse_item(Parser *parser);
static bool parse_space(Parser *parser);

// Rule functions
static bool parse_1(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "1", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Capture Table
      int initial_stack_size = lua_gettop(parser->L);
      { // Zero or more repetitions
        while (true) {
          parse_item(parser);
          if (!parser->success) {
            break;
          }
        }
        parser->success = true;
      }

      if (parser->success) {
        int new_stack_size = lua_gettop(parser->L);
        int items_count = new_stack_size - initial_stack_size;
        int table_position = -items_count - 1;

        lua_createtable(parser->L, items_count, 0);

        lua_insert(parser->L, table_position);

        for (int i = items_count; i >= 1; --i) {
          lua_rawseti(parser->L, table_position, i);
          table_position += 1;
        }
      }
    }

    if (parser->success) {
      { // Negate (only match if pattern fails)
        REMEMBER_POSITION(parser, pos);

        { // Match any 1 characters
          if (parser->pos + 1 <= parser->input_len) {
            parser->pos += 1;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected at least 1 more characters at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (parser->success) {
          // Pattern matched, so negate fails
          RESTORE_POSITION(parser, pos);
          parser->success = false;
#ifdef PGEN_ERRORS
          sprintf(parser->error_message,
                  "Negated pattern unexpectedly matched at position %zu",
                  pos.pos);
#endif
        } else {
          // Pattern failed, so negate succeeds
          parser->success = true;
          RESTORE_POSITION(
              parser,
              pos); // Restore original position (technically not necessary
                    // since failed pattern should make no changes to position)
        }
      }

      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "1", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "1", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_item(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "item", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Capture
      size_t start_pos = parser->pos;
      { // At least 1 repetitions
        REMEMBER_POSITION(parser, pos);
        size_t rep_count = 0;

        while (true) {
          { // Match character range: "0"-"9", "a"-"z", "A"-"Z"
            if (parser->pos < parser->input_len &&
                ((parser->input[parser->pos] >= 48 &&
                  parser->input[parser->pos] <= 57) ||
                 (parser->input[parser->pos] >= 97 &&
                  parser->input[parser->pos] <= 122) ||
                 (parser->input[parser->pos] >= 65 &&
                  parser->input[parser->pos] <= 90))) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(
                  parser->error_message,
                  "Expected character in ranges [" 0 "-" 9 ", " a "-" z ", " A
                                                           "-" Z
                                                           "] at position %zu",
                  parser->pos);
#endif
              parser->success = false;
            }
          }

          if (!parser->success) {
            break;
          }

          rep_count += 1;
        }

        if (rep_count >= 1) {
          parser->success = true;
        } else {
          RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
          sprintf(parser->error_message,
                  "Expected 1 repetitions at position %zu", parser->pos);
#endif
        }
      }

      if (parser->success) {
        size_t capture_length = parser->pos - start_pos;
        // TODO: ensure stack has enough space for push
        lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
      }
    }

    if (parser->success) {
      { // At most 1 repetitions
        size_t rep_count = 0;

        while (rep_count < 1) {
          size_t before_pos = parser->pos;

          {
            parse_space(parser);
          }

          if (!parser->success || before_pos == parser->pos) {
            // Break on failure or zero-width match
            parser->success = true;
            break;
          }

          rep_count += 1;
        }
      }

      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "item", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "item", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_space(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "space", start);
#endif

  { // Match character set " \t\n\r"
    if (parser->pos < parser->input_len) {
      switch (parser->input[parser->pos]) {
      case 32: /* " " */
      case 9:  /* "\t" */
      case 10: /* "\n" */
      case 13: /* "\r" */
        parser->pos++;
        break;
      default:
#ifdef PGEN_ERRORS
        sprintf(parser->error_message,
                "Expected one of "
                "\" \\t\\n\\r\""
                " at position %zu",
                parser->pos);
#endif
        parser->success = false;
      }
    } else {
#ifdef PGEN_ERRORS
      sprintf(parser->error_message,
              "Expected one of "
              "\" \\t\\n\\r\""
              " at position %zu but reached end of input",
              parser->pos);
#endif
      parser->success = false;
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "space", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "space", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

// Initialize parser
static Parser *range_init(const char *input, lua_State *L) {
  Parser *parser = (Parser *)malloc(sizeof(Parser));
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
static void range_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_range_parse(lua_State *L) {
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
  Parser *parser = range_init(input, L);
  if (!parser) {
    lua_pushnil(L);
    lua_pushstring(L, "Parser initialization failed (memory allocation?)");
    return 2;
  }

  int initial_stack_size = lua_gettop(parser->L);

  parse_1(parser);

  int final_stack_size = lua_gettop(parser->L);

  // Return nil and error message on failure, true on success
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size &&
           "Unexpected stack size change on parse failure.");
    lua_pushnil(L);
    lua_pushstring(L, parser->error_message);
    range_free(parser);
    return 2; // Return nil and error message
  }

  // If stack size has changed, use new items as return values
  if (final_stack_size > initial_stack_size) {
    range_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  range_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg range_module[] = {
    {"parse", l_range_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}              // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_range(lua_State *L) {
  luaL_newlib(L, range_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register
int luaopen_range(lua_State *L) {
  luaL_register(
      L, "range",
      range_module); // Registers functions in global table (or package table)
  return 1;
}
#endif
/*
To compile as a Lua module:
gcc -shared -o range.so -fPIC range.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local range = require "range"
local result = range.parse("your input string")
*/

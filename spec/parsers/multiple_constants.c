#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// multiple_constants - generated parser

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
static bool parse_main(Parser *parser);

// Rule functions
static bool parse_main(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "main", start);
#endif

  { // Sequence with 5 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match literal "test"
      if (parser->pos + 4 <= parser->input_len &&
          memcmp(parser->input + parser->pos, "test", 4) == 0) {
        parser->pos += 4;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message,
                "Expected `"
                "test"
                "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
      }
    }
    if (parser->success) {
      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushnumber(parser->L, 42);
      }
      if (parser->success) {
        { // Constant Capture
          // A constant capture matches the empty string and produces all given
          // values
          lua_pushlstring(parser->L, "test_field", 10);
        }
        if (parser->success) {
          { // Constant Capture
            // A constant capture matches the empty string and produces all
            // given values
            lua_pushnil(parser->L);
          }
          if (parser->success) {
            { // Constant Capture
              // A constant capture matches the empty string and produces all
              // given values
              lua_pushboolean(parser->L, 1);
            }
          }
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
            "", "main", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "main", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

// Initialize parser
static Parser *multiple_constants_init(const char *input, lua_State *L) {
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
static void multiple_constants_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_multiple_constants_parse(lua_State *L) {
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
  Parser *parser = multiple_constants_init(input, L);
  if (!parser) {
    lua_pushnil(L);
    lua_pushstring(L, "Parser initialization failed (memory allocation?)");
    return 2;
  }

  int initial_stack_size = lua_gettop(parser->L);

  parse_main(parser);

  int final_stack_size = lua_gettop(parser->L);

  // Return nil and error message on failure, true on success
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size &&
           "Unexpected stack size change on parse failure.");
    lua_pushnil(L);
    lua_pushstring(L, parser->error_message);
    multiple_constants_free(parser);
    return 2; // Return nil and error message
  }

  // If stack size has changed, use new items as return values
  if (final_stack_size > initial_stack_size) {
    multiple_constants_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  multiple_constants_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg multiple_constants_module[] = {
    {"parse",
     l_multiple_constants_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}                  // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_multiple_constants(lua_State *L) {
  luaL_newlib(
      L, multiple_constants_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register
int luaopen_multiple_constants(lua_State *L) {
  luaL_register(L, "multiple_constants",
                multiple_constants_module); // Registers functions in global
                                            // table (or package table)
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o multiple_constants.so -fPIC multiple_constants.c `pkg-config
--cflags --libs lua5.1`

To use in Lua:
local multiple_constants = require "multiple_constants"
local result = multiple_constants.parse("your input string")
*/

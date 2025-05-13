#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// lookahead - generated parser

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
static bool parse_positive(Parser *parser);
static bool parse_negative(Parser *parser);

// Rule functions
static bool parse_1(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "1", start);
#endif

  { // Choice
    parse_positive(parser);

    if (!parser->success) {
      parser->success = true;
      parse_negative(parser);
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

static bool parse_positive(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "positive", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Sequence
      REMEMBER_POSITION(parser, pos);

      { // Sequence
        REMEMBER_POSITION(parser, pos);

        { // Match literal "abc"
          if (parser->pos + 3 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "abc", 3) == 0) {
            parser->pos += 3;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected `"
                    "abc"
                    "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (parser->success) {
          { // Lookahead (match without consuming input)
            REMEMBER_POSITION(parser, pos);

            { // Capture
              size_t start_pos = parser->pos;
              { // Match literal "def"
                if (parser->pos + 3 <= parser->input_len &&
                    memcmp(parser->input + parser->pos, "def", 3) == 0) {
                  parser->pos += 3;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message,
                          "Expected `"
                          "def"
                          "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }

              if (parser->success) {
                size_t capture_length = parser->pos - start_pos;
                // TODO: ensure stack has enough space for push
                lua_pushlstring(parser->L, parser->input + start_pos,
                                capture_length);
              }
            }

            if (parser->success) {
              // Pattern matched, but we don't consume any input
              RESTORE_POSITION(parser, pos);
            }
          }

          if (!parser->success) {
            RESTORE_POSITION(parser, pos);
          }
        }
      }

      if (parser->success) {
        { // Match literal "def"
          if (parser->pos + 3 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "def", 3) == 0) {
            parser->pos += 3;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected `"
                    "def"
                    "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
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
            "", "positive", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "positive", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_negative(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "negative", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Match literal "xyz"
      if (parser->pos + 3 <= parser->input_len &&
          memcmp(parser->input + parser->pos, "xyz", 3) == 0) {
        parser->pos += 3;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message,
                "Expected `"
                "xyz"
                "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
      }
    }

    if (parser->success) {
      { // Lookahead (match without consuming input)
        REMEMBER_POSITION(parser, pos);

        { // Negate (only match if pattern fails)
          REMEMBER_POSITION(parser, pos);

          { // Match literal "def"
            if (parser->pos + 3 <= parser->input_len &&
                memcmp(parser->input + parser->pos, "def", 3) == 0) {
              parser->pos += 3;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected `"
                      "def"
                      "` at position %zu",
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
            RESTORE_POSITION(parser,
                             pos); // Restore original position (technically not
                                   // necessary since failed pattern should make
                                   // no changes to position)
          }
        }

        if (parser->success) {
          // Pattern matched, but we don't consume any input
          RESTORE_POSITION(parser, pos);
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
            "", "negative", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "negative", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

// Initialize parser
static Parser *lookahead_init(const char *input, lua_State *L) {
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
static void lookahead_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_lookahead_parse(lua_State *L) {
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
  Parser *parser = lookahead_init(input, L);
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
    lookahead_free(parser);
    return 2; // Return nil and error message
  }

  // If stack size has changed, use new items as return values
  if (final_stack_size > initial_stack_size) {
    lookahead_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  lookahead_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg lookahead_module[] = {
    {"parse", l_lookahead_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}                  // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_lookahead(lua_State *L) {
  luaL_newlib(L, lookahead_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register
int luaopen_lookahead(lua_State *L) {
  luaL_register(L, "lookahead",
                lookahead_module); // Registers functions in global table (or
                                   // package table)
  return 1;
}
#endif
/*
To compile as a Lua module:
gcc -shared -o lookahead.so -fPIC lookahead.c `pkg-config --cflags --libs
lua5.1`

To use in Lua:
local lookahead = require "lookahead"
local result = lookahead.parse("your input string")
*/

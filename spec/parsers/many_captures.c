#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// many_captures - generated parser

// Maximum rule-call recursion depth before the parse is aborted with a Lua
// error (prevents C stack overflow on deeply nested input). Override with
// the max_depth compile option or -DPGEN_MAX_DEPTH=n
#ifndef PGEN_MAX_DEPTH
#define PGEN_MAX_DEPTH 5000
#endif

typedef struct {
  const char *input;
  size_t input_len;
  size_t pos;
  bool success;
  char error_message[256];
  const char *throw_label; // Label from T() or NULL for ordinary failure
  size_t throw_pos;        // Position where T() was thrown
  size_t depth;
  lua_State *L;
} Parser;

typedef struct {
  size_t pos;
  int stack_size;
} ParserPosition;

#define REMEMBER_POSITION(parser, pp) \
  ParserPosition pp;                  \
  (pp).pos = (parser)->pos;           \
  (pp).stack_size = lua_gettop((parser)->L);

// Restore parser position
#define RESTORE_POSITION(parser, pp) \
  (parser)->pos = (pp).pos;          \
  lua_settop((parser)->L, (pp).stack_size);

// Ensure the Lua stack can hold n more values. Captures are built on the Lua
// stack, so without this a large parse tree would overflow it (undefined
// behavior). Raises a Lua error when the stack cannot grow any further
// (LUAI_MAXCSTACK).
static void pgen_checkstack(Parser *parser, int n) {
  if (!lua_checkstack(parser->L, n)) {
    luaL_error(parser->L, "pgen: Lua stack overflow while building captures");
  }
}

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

// No Cg sentinels defined - stub function
static bool is_cg_sentinel(void *ptr) {
  (void)ptr; // unused
  return false;
}

// Forward declarations
static bool parse_test(Parser *parser);
static bool parse_collected(Parser *parser);
static bool parse_flat(Parser *parser);

// Rule functions
static bool parse_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test", start);
#endif

  {   // Choice
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Match literal "1:"
        if (parser->pos + 2 <= parser->input_len &&
            memcmp(parser->input + parser->pos, "1:", 2) == 0) {
          parser->pos += 2;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected `"
                                         "1:"
                                         "` at position %zu",
                  parser->pos);
#endif
          parser->success = false;
        }
      }
      if (parser->success) {
        parse_flat(parser);
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    // Only try alternative if ordinary failure (not labeled failure from T())
    if (!parser->success && !parser->throw_label) {
      parser->success = true;
      { // Sequence with 2 patterns
        REMEMBER_POSITION(parser, pos);

        { // Match literal "2:"
          if (parser->pos + 2 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "2:", 2) == 0) {
            parser->pos += 2;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected `"
                                           "2:"
                                           "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }
        if (parser->success) {
          parse_collected(parser);
          if (!parser->success) {
            RESTORE_POSITION(parser, pos);
          }
        }
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_collected(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "collected", start);
#endif

  { // Capture Table (array-only)
    int initial_stack_size = lua_gettop(parser->L);
    { // Zero or more repetitions
      while (true) {
        { // Capture
          size_t start_pos = parser->pos;
          { // Match single character "a"
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 97) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected character `"
                                             "a"
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          if (parser->success) {
            size_t capture_length = parser->pos - start_pos;
            pgen_checkstack(parser, 1);
            lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
          }
        }
        if (!parser->success) {
          break;
        }
      }
      // Only recover from ordinary failure, not labeled failure from T()
      if (!parser->throw_label) {
        parser->success = true;
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_start = initial_stack_size + 1;
      int array_count = new_stack_size - initial_stack_size;

      pgen_checkstack(parser, 2); // table + one temporary during fill
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
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "collected", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "collected", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_flat(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "flat", start);
#endif

  { // Zero or more repetitions
    while (true) {
      { // Capture
        size_t start_pos = parser->pos;
        { // Match single character "a"
          if (parser->pos < parser->input_len &&
              parser->input[parser->pos] == 97) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected character `"
                                           "a"
                                           "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (parser->success) {
          size_t capture_length = parser->pos - start_pos;
          pgen_checkstack(parser, 1);
          lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
        }
      }
      if (!parser->success) {
        break;
      }
    }
    // Only recover from ordinary failure, not labeled failure from T()
    if (!parser->throw_label) {
      parser->success = true;
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "flat", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "flat", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

// Initialize parser
static Parser *many_captures_init(const char *input, lua_State *L) {
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
  parser->throw_label = NULL;
  parser->throw_pos = 0;
  parser->L = L;
  return parser;
}

// Free parser
static void many_captures_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_many_captures_parse(lua_State *L) {
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
  Parser *parser = many_captures_init(input, L);
  if (!parser) {
    lua_pushnil(L);
    lua_pushstring(L, "Parser initialization failed (memory allocation?)");
    return 2;
  }

  int initial_stack_size = lua_gettop(parser->L);

  parse_test(parser);

  int final_stack_size = lua_gettop(parser->L);

  // Return nil and error info on failure
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size && "Unexpected stack size change on parse failure.");
    lua_pushnil(L);
    if (parser->throw_label) {
      // Labeled failure: return nil, label, position
      lua_pushstring(L, parser->throw_label);
      lua_pushinteger(L, parser->throw_pos + 1); // 1-indexed for Lua
      many_captures_free(parser);
      return 3;
    } else {
      // Ordinary failure: return nil, error_message
      lua_pushstring(L, parser->error_message);
      many_captures_free(parser);
      return 2;
    }
  }

  // Strip Cg sentinel+value pairs from stack (they only matter inside Ct)
  if (final_stack_size > initial_stack_size) {
    lua_checkstack(L, 1); // one temporary during compaction
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
    many_captures_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  many_captures_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg many_captures_module[] = {
    {"parse", l_many_captures_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}                      // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_many_captures(lua_State *L) {

  luaL_newlib(L, many_captures_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register
int luaopen_many_captures(lua_State *L) {

  luaL_register(L, "many_captures", many_captures_module); // Registers functions in global table (or package table)
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o many_captures.so -fPIC many_captures.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local many_captures = require "many_captures"
local result = many_captures.parse("your input string")
*/

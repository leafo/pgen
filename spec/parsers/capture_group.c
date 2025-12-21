#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// capture_group - generated parser

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
  ParserPosition pp;                  \
  (pp).pos = (parser)->pos;           \
  (pp).stack_size = lua_gettop((parser)->L);

// Restore parser position
#define RESTORE_POSITION(parser, pp) \
  (parser)->pos = (pp).pos;          \
  lua_settop((parser)->L, (pp).stack_size);

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

// Named capture group sentinels
static const char __cg_sentinel_const_field[] = "const_field";
static const char __cg_sentinel_data[] = "data";
static const char __cg_sentinel_first[] = "first";
static const char __cg_sentinel_greeting[] = "greeting";
static const char __cg_sentinel_named[] = "named";
static const char __cg_sentinel_second[] = "second";

// Registry of all known sentinels (for validation)
static const void *__cg_sentinel_registry[] = {
    (void *)__cg_sentinel_const_field,
    (void *)__cg_sentinel_data,
    (void *)__cg_sentinel_first,
    (void *)__cg_sentinel_greeting,
    (void *)__cg_sentinel_named,
    (void *)__cg_sentinel_second,
    NULL // terminator
};

// Check if a pointer is one of our Cg sentinels
static bool is_cg_sentinel(void *ptr) {
  for (int i = 0; __cg_sentinel_registry[i] != NULL; i++) {
    if (ptr == __cg_sentinel_registry[i])
      return true;
  }
  return false;
}

// Forward declarations
static bool parse_test(Parser *parser);
static bool parse_test1(Parser *parser);
static bool parse_test2(Parser *parser);
static bool parse_test3(Parser *parser);
static bool parse_test4(Parser *parser);
static bool parse_test5(Parser *parser);
static bool parse_test6(Parser *parser);

// Rule functions
static bool parse_test(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test", start);
#endif

  {           // Choice
    {         // Choice
      {       // Choice
        {     // Choice
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
                parse_test1(parser);
                if (!parser->success) {
                  RESTORE_POSITION(parser, pos);
                }
              }
            }

            if (!parser->success) {
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
                  parse_test2(parser);
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }
            }
          }

          if (!parser->success) {
            parser->success = true;
            { // Sequence with 2 patterns
              REMEMBER_POSITION(parser, pos);

              { // Match literal "3:"
                if (parser->pos + 2 <= parser->input_len &&
                    memcmp(parser->input + parser->pos, "3:", 2) == 0) {
                  parser->pos += 2;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected `"
                                                 "3:"
                                                 "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }
              if (parser->success) {
                parse_test3(parser);
                if (!parser->success) {
                  RESTORE_POSITION(parser, pos);
                }
              }
            }
          }
        }

        if (!parser->success) {
          parser->success = true;
          { // Sequence with 2 patterns
            REMEMBER_POSITION(parser, pos);

            { // Match literal "4:"
              if (parser->pos + 2 <= parser->input_len &&
                  memcmp(parser->input + parser->pos, "4:", 2) == 0) {
                parser->pos += 2;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Expected `"
                                               "4:"
                                               "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            }
            if (parser->success) {
              parse_test4(parser);
              if (!parser->success) {
                RESTORE_POSITION(parser, pos);
              }
            }
          }
        }
      }

      if (!parser->success) {
        parser->success = true;
        { // Sequence with 2 patterns
          REMEMBER_POSITION(parser, pos);

          { // Match literal "5:"
            if (parser->pos + 2 <= parser->input_len &&
                memcmp(parser->input + parser->pos, "5:", 2) == 0) {
              parser->pos += 2;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected `"
                                             "5:"
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }
          if (parser->success) {
            parse_test5(parser);
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }
      }
    }

    if (!parser->success) {
      parser->success = true;
      { // Sequence with 2 patterns
        REMEMBER_POSITION(parser, pos);

        { // Match literal "6:"
          if (parser->pos + 2 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "6:", 2) == 0) {
            parser->pos += 2;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected `"
                                           "6:"
                                           "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }
        if (parser->success) {
          parse_test6(parser);
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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_test1(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test1", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Capture Group "greeting"
        int cg_stack_start = lua_gettop(parser->L);
        size_t start_pos = parser->pos;
        { // Match literal "hello"
          if (parser->pos + 5 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "hello", 5) == 0) {
            parser->pos += 5;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected `"
                                           "hello"
                                           "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (parser->success) {
          int cg_stack_end = lua_gettop(parser->L);
          int captures_produced = cg_stack_end - cg_stack_start;

          if (captures_produced > 0) {
            // Inner pattern produced captures - use the first one
            // Push sentinel (identifies this as named capture "greeting")
            lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_greeting);
            // Move sentinel before the first capture
            lua_insert(parser->L, cg_stack_start + 1);
            // Now stack is: sentinel, first_capture, [other_captures...]
            // Remove any extra captures (keep only sentinel + first capture)
            lua_settop(parser->L, cg_stack_start + 2);
          } else {
            // No captures - capture the matched text span
            size_t capture_len = parser->pos - start_pos;
            // Push sentinel (identifies this as named capture "greeting")
            lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_greeting);
            // Push captured value
            lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
          }
        }
      }
      if (parser->success) {
        { // Match single character " "
          if (parser->pos < parser->input_len &&
              parser->input[parser->pos] == 32) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected character `"
                                           " "
                                           "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }
        if (parser->success) {
          { // Capture
            size_t start_pos = parser->pos;
            { // Match literal "world"
              if (parser->pos + 5 <= parser->input_len &&
                  memcmp(parser->input + parser->pos, "world", 5) == 0) {
                parser->pos += 5;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Expected `"
                                               "world"
                                               "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            }

            if (parser->success) {
              size_t capture_length = parser->pos - start_pos;
              // TODO: ensure stack has enough space for push
              lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

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
          void *ptr = lua_touserdata(parser->L, i);
          if (is_cg_sentinel(ptr)) {
            // Named capture: sentinel at i, value at i+1
            const char *name = (const char *)ptr;
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
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test1", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test1", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_test2(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test2", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Capture Group "first"
        int cg_stack_start = lua_gettop(parser->L);
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
          int cg_stack_end = lua_gettop(parser->L);
          int captures_produced = cg_stack_end - cg_stack_start;

          if (captures_produced > 0) {
            // Inner pattern produced captures - use the first one
            // Push sentinel (identifies this as named capture "first")
            lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_first);
            // Move sentinel before the first capture
            lua_insert(parser->L, cg_stack_start + 1);
            // Now stack is: sentinel, first_capture, [other_captures...]
            // Remove any extra captures (keep only sentinel + first capture)
            lua_settop(parser->L, cg_stack_start + 2);
          } else {
            // No captures - capture the matched text span
            size_t capture_len = parser->pos - start_pos;
            // Push sentinel (identifies this as named capture "first")
            lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_first);
            // Push captured value
            lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
          }
        }
      }
      if (parser->success) {
        { // Capture Group "second"
          int cg_stack_start = lua_gettop(parser->L);
          size_t start_pos = parser->pos;
          { // Match single character "b"
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 98) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected character `"
                                             "b"
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          if (parser->success) {
            int cg_stack_end = lua_gettop(parser->L);
            int captures_produced = cg_stack_end - cg_stack_start;

            if (captures_produced > 0) {
              // Inner pattern produced captures - use the first one
              // Push sentinel (identifies this as named capture "second")
              lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_second);
              // Move sentinel before the first capture
              lua_insert(parser->L, cg_stack_start + 1);
              // Now stack is: sentinel, first_capture, [other_captures...]
              // Remove any extra captures (keep only sentinel + first capture)
              lua_settop(parser->L, cg_stack_start + 2);
            } else {
              // No captures - capture the matched text span
              size_t capture_len = parser->pos - start_pos;
              // Push sentinel (identifies this as named capture "second")
              lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_second);
              // Push captured value
              lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

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
          void *ptr = lua_touserdata(parser->L, i);
          if (is_cg_sentinel(ptr)) {
            // Named capture: sentinel at i, value at i+1
            const char *name = (const char *)ptr;
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
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test2", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test2", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_test3(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test3", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Capture
        size_t start_pos = parser->pos;
        { // Match single character "x"
          if (parser->pos < parser->input_len &&
              parser->input[parser->pos] == 120) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected character `"
                                           "x"
                                           "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (parser->success) {
          size_t capture_length = parser->pos - start_pos;
          // TODO: ensure stack has enough space for push
          lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
        }
      }
      if (parser->success) {
        { // Capture Group "named"
          int cg_stack_start = lua_gettop(parser->L);
          size_t start_pos = parser->pos;
          { // Match single character "y"
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 121) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected character `"
                                             "y"
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          if (parser->success) {
            int cg_stack_end = lua_gettop(parser->L);
            int captures_produced = cg_stack_end - cg_stack_start;

            if (captures_produced > 0) {
              // Inner pattern produced captures - use the first one
              // Push sentinel (identifies this as named capture "named")
              lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_named);
              // Move sentinel before the first capture
              lua_insert(parser->L, cg_stack_start + 1);
              // Now stack is: sentinel, first_capture, [other_captures...]
              // Remove any extra captures (keep only sentinel + first capture)
              lua_settop(parser->L, cg_stack_start + 2);
            } else {
              // No captures - capture the matched text span
              size_t capture_len = parser->pos - start_pos;
              // Push sentinel (identifies this as named capture "named")
              lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_named);
              // Push captured value
              lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
            }
          }
        }
        if (parser->success) {
          { // Capture
            size_t start_pos = parser->pos;
            { // Match single character "z"
              if (parser->pos < parser->input_len &&
                  parser->input[parser->pos] == 122) {
                parser->pos++;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Expected character `"
                                               "z"
                                               "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            }

            if (parser->success) {
              size_t capture_length = parser->pos - start_pos;
              // TODO: ensure stack has enough space for push
              lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

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
          void *ptr = lua_touserdata(parser->L, i);
          if (is_cg_sentinel(ptr)) {
            // Named capture: sentinel at i, value at i+1
            const char *name = (const char *)ptr;
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
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test3", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test3", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_test4(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test4", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Capture Group "const_field"
        int cg_stack_start = lua_gettop(parser->L);
        size_t start_pos = parser->pos;
        { // Constant Capture
          // A constant capture matches the empty string and produces all given values
          lua_pushlstring(parser->L, "constant_value", 14);
        }

        if (parser->success) {
          int cg_stack_end = lua_gettop(parser->L);
          int captures_produced = cg_stack_end - cg_stack_start;

          if (captures_produced > 0) {
            // Inner pattern produced captures - use the first one
            // Push sentinel (identifies this as named capture "const_field")
            lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_const_field);
            // Move sentinel before the first capture
            lua_insert(parser->L, cg_stack_start + 1);
            // Now stack is: sentinel, first_capture, [other_captures...]
            // Remove any extra captures (keep only sentinel + first capture)
            lua_settop(parser->L, cg_stack_start + 2);
          } else {
            // No captures - capture the matched text span
            size_t capture_len = parser->pos - start_pos;
            // Push sentinel (identifies this as named capture "const_field")
            lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_const_field);
            // Push captured value
            lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
          }
        }
      }
      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          { // Match literal "hello"
            if (parser->pos + 5 <= parser->input_len &&
                memcmp(parser->input + parser->pos, "hello", 5) == 0) {
              parser->pos += 5;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected `"
                                             "hello"
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          if (parser->success) {
            size_t capture_length = parser->pos - start_pos;
            // TODO: ensure stack has enough space for push
            lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

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
          void *ptr = lua_touserdata(parser->L, i);
          if (is_cg_sentinel(ptr)) {
            // Named capture: sentinel at i, value at i+1
            const char *name = (const char *)ptr;
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
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test4", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test4", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_test5(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test5", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        lua_pushlstring(parser->L, "type", 4);
      }
      if (parser->success) {
        { // Capture Group "data"
          int cg_stack_start = lua_gettop(parser->L);
          size_t start_pos = parser->pos;
          { // Match literal "value"
            if (parser->pos + 5 <= parser->input_len &&
                memcmp(parser->input + parser->pos, "value", 5) == 0) {
              parser->pos += 5;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected `"
                                             "value"
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          if (parser->success) {
            int cg_stack_end = lua_gettop(parser->L);
            int captures_produced = cg_stack_end - cg_stack_start;

            if (captures_produced > 0) {
              // Inner pattern produced captures - use the first one
              // Push sentinel (identifies this as named capture "data")
              lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_data);
              // Move sentinel before the first capture
              lua_insert(parser->L, cg_stack_start + 1);
              // Now stack is: sentinel, first_capture, [other_captures...]
              // Remove any extra captures (keep only sentinel + first capture)
              lua_settop(parser->L, cg_stack_start + 2);
            } else {
              // No captures - capture the matched text span
              size_t capture_len = parser->pos - start_pos;
              // Push sentinel (identifies this as named capture "data")
              lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_data);
              // Push captured value
              lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

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
          void *ptr = lua_touserdata(parser->L, i);
          if (is_cg_sentinel(ptr)) {
            // Named capture: sentinel at i, value at i+1
            const char *name = (const char *)ptr;
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
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test5", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test5", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_test6(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test6", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Capture Group "greeting"
      int cg_stack_start = lua_gettop(parser->L);
      size_t start_pos = parser->pos;
      { // Sequence with 3 patterns
        REMEMBER_POSITION(parser, pos);

        { // Capture
          size_t start_pos = parser->pos;
          { // Match literal "hi"
            if (parser->pos + 2 <= parser->input_len &&
                memcmp(parser->input + parser->pos, "hi", 2) == 0) {
              parser->pos += 2;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected `"
                                             "hi"
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          if (parser->success) {
            size_t capture_length = parser->pos - start_pos;
            // TODO: ensure stack has enough space for push
            lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
          }
        }
        if (parser->success) {
          { // Match single character " "
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 32) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected character `"
                                             " "
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }
          if (parser->success) {
            { // Capture
              size_t start_pos = parser->pos;
              { // Match literal "there"
                if (parser->pos + 5 <= parser->input_len &&
                    memcmp(parser->input + parser->pos, "there", 5) == 0) {
                  parser->pos += 5;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected `"
                                                 "there"
                                                 "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }

              if (parser->success) {
                size_t capture_length = parser->pos - start_pos;
                // TODO: ensure stack has enough space for push
                lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
              }
            }
          }
          if (!parser->success) {
            RESTORE_POSITION(parser, pos);
          }
        }
      }

      if (parser->success) {
        int cg_stack_end = lua_gettop(parser->L);
        int captures_produced = cg_stack_end - cg_stack_start;

        if (captures_produced > 0) {
          // Inner pattern produced captures - use the first one
          // Push sentinel (identifies this as named capture "greeting")
          lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_greeting);
          // Move sentinel before the first capture
          lua_insert(parser->L, cg_stack_start + 1);
          // Now stack is: sentinel, first_capture, [other_captures...]
          // Remove any extra captures (keep only sentinel + first capture)
          lua_settop(parser->L, cg_stack_start + 2);
        } else {
          // No captures - capture the matched text span
          size_t capture_len = parser->pos - start_pos;
          // Push sentinel (identifies this as named capture "greeting")
          lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_greeting);
          // Push captured value
          lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
        }
      }
    }

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
          void *ptr = lua_touserdata(parser->L, i);
          if (is_cg_sentinel(ptr)) {
            // Named capture: sentinel at i, value at i+1
            const char *name = (const char *)ptr;
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
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test6", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test6", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

// Initialize parser
static Parser *capture_group_init(const char *input, lua_State *L) {
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
static void capture_group_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_capture_group_parse(lua_State *L) {
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
  Parser *parser = capture_group_init(input, L);
  if (!parser) {
    lua_pushnil(L);
    lua_pushstring(L, "Parser initialization failed (memory allocation?)");
    return 2;
  }

  int initial_stack_size = lua_gettop(parser->L);

  parse_test(parser);

  int final_stack_size = lua_gettop(parser->L);

  // Return nil and error message on failure, true on success
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size && "Unexpected stack size change on parse failure.");
    lua_pushnil(L);
    lua_pushstring(L, parser->error_message);
    capture_group_free(parser);
    return 2; // Return nil and error message
  }

  // If stack size has changed, use new items as return values
  if (final_stack_size > initial_stack_size) {
    capture_group_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  capture_group_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg capture_group_module[] = {
    {"parse", l_capture_group_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}                      // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_capture_group(lua_State *L) {
  luaL_newlib(L, capture_group_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register
int luaopen_capture_group(lua_State *L) {
  luaL_register(L, "capture_group", capture_group_module); // Registers functions in global table (or package table)
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o capture_group.so -fPIC capture_group.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local capture_group = require "capture_group"
local result = capture_group.parse("your input string")
*/

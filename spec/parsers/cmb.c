#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// cmb - generated parser

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
  size_t furthest_fail;    // Furthest position where a match attempt failed
  size_t depth;
  lua_State *L;
} Parser;

typedef struct {
  size_t pos;
  int stack_size;
} ParserPosition;

typedef struct {
  size_t pos;
} ParserInputPosition;

#define REMEMBER_POSITION(parser, pp) \
  ParserPosition pp;                  \
  (pp).pos = (parser)->pos;           \
  (pp).stack_size = lua_gettop((parser)->L);

// Restore parser position
#define RESTORE_POSITION(parser, pp) \
  (parser)->pos = (pp).pos;          \
  lua_settop((parser)->L, (pp).stack_size);

#define REMEMBER_INPUT_POSITION(parser, pp) \
  ParserInputPosition pp;                   \
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
#define PGEN_RECORD_FURTHEST(parser)             \
  do {                                           \
    if ((parser)->pos > (parser)->furthest_fail) \
      (parser)->furthest_fail = (parser)->pos;   \
  } while (0)
#endif

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

// Named capture group sentinels
static const char __cg_sentinel_as[] = "as";
static const char __cg_sentinel_eq[] = "eq";
static const char __cg_sentinel_maybe[] = "maybe";
static const char __cg_sentinel_x[] = "x";

// Registry of all known sentinels (for validation)
static const void *__cg_sentinel_registry[] = {
    (void *)__cg_sentinel_as,
    (void *)__cg_sentinel_eq,
    (void *)__cg_sentinel_maybe,
    (void *)__cg_sentinel_x,
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
static bool parse_basic_match(Parser *parser);
static bool parse_empty_match(Parser *parser);
static bool parse_lua_long_string(Parser *parser);
static bool parse_mismatch(Parser *parser);

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
              PGEN_RECORD_FURTHEST(parser);
            }
          }
          if (parser->success) {
            parse_basic_match(parser);
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
                PGEN_RECORD_FURTHEST(parser);
              }
            }
            if (parser->success) {
              parse_lua_long_string(parser);
              if (!parser->success) {
                RESTORE_POSITION(parser, pos);
              }
            }
          }
        }
      }

      // Only try alternative if ordinary failure (not labeled failure from T())
      if (!parser->success && !parser->throw_label) {
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
              PGEN_RECORD_FURTHEST(parser);
            }
          }
          if (parser->success) {
            parse_empty_match(parser);
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }
      }
    }

    // Only try alternative if ordinary failure (not labeled failure from T())
    if (!parser->success && !parser->throw_label) {
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
            PGEN_RECORD_FURTHEST(parser);
          }
        }
        if (parser->success) {
          parse_mismatch(parser);
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

static bool parse_basic_match(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "basic_match", start);
#endif

  { // Sequence with 3 patterns
    REMEMBER_POSITION(parser, pos);

    { // Capture Group "as"
      int cg_stack_start = lua_gettop(parser->L);
      size_t start_pos = parser->pos;
      { // At least 1 repetitions
        REMEMBER_INPUT_POSITION(parser, pos);
        size_t rep_count = 0;

        while (true) {
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

          if (!parser->success) {
            break;
          }

          rep_count += 1;
        }

        // Don't recover if labeled failure was thrown
        if (parser->throw_label) {
          // Keep failure state, propagate labeled failure
        } else if (rep_count >= 1) {
          parser->success = true;
        } else {
          RESTORE_INPUT_POSITION(parser, pos);
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected 1 repetitions at position %zu", parser->pos);
#endif
        }
      }

      if (parser->success) {
        int cg_stack_end = lua_gettop(parser->L);
        int captures_produced = cg_stack_end - cg_stack_start;

        pgen_checkstack(parser, 2); // sentinel + value
        if (captures_produced > 0) {
          // Inner pattern produced captures - use the first one
          // Push sentinel (identifies this as named capture "as")
          lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_as);
          // Move sentinel before the first capture
          lua_insert(parser->L, cg_stack_start + 1);
          // Now stack is: sentinel, first_capture, [other_captures...]
          // Remove any extra captures (keep only sentinel + first capture)
          lua_settop(parser->L, cg_stack_start + 2);
        } else {
          // No captures - capture the matched text span
          size_t capture_len = parser->pos - start_pos;
          // Push sentinel (identifies this as named capture "as")
          lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_as);
          // Push captured value
          lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
        }
      }
    }
    if (parser->success) {
      { // Match single character ":"
        if (parser->pos < parser->input_len &&
            parser->input[parser->pos] == 58) {
          parser->pos++;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected character `"
                                         ":"
                                         "` at position %zu",
                  parser->pos);
#endif
          parser->success = false;
        }
      }
      if (parser->success) {
        { // Capture Match Back "as"
          parser->success = false;

          // Search backward through stack for sentinel "as"
          for (int i = lua_gettop(parser->L); i >= 1; i--) {
            if (lua_islightuserdata(parser->L, i)) {
              void *ptr = lua_touserdata(parser->L, i);
              if (ptr == (void *)__cg_sentinel_as) {
                // Found sentinel - value is at i+1 (use only if it's already a string)
                if (i + 1 <= lua_gettop(parser->L) && lua_type(parser->L, i + 1) == LUA_TSTRING) {
                  size_t match_len;
                  const char *match_str = lua_tolstring(parser->L, i + 1, &match_len);
                  // Try to match at current position
                  if (parser->pos + match_len <= parser->input_len &&
                      memcmp(parser->input + parser->pos, match_str, match_len) == 0) {
                    parser->pos += match_len;
                    parser->success = true;
                  }
                }
                break; // Found sentinel, stop searching regardless of match result
              }
            }
          }
          if (!parser->success) {
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Capture match back 'as' failed at position %zu", parser->pos);
#endif
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "basic_match", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "basic_match", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_empty_match(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "empty_match", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Capture Group "maybe"
        int cg_stack_start = lua_gettop(parser->L);
        size_t start_pos = parser->pos;
        { // Zero or more repetitions
          while (true) {
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
          int cg_stack_end = lua_gettop(parser->L);
          int captures_produced = cg_stack_end - cg_stack_start;

          pgen_checkstack(parser, 2); // sentinel + value
          if (captures_produced > 0) {
            // Inner pattern produced captures - use the first one
            // Push sentinel (identifies this as named capture "maybe")
            lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_maybe);
            // Move sentinel before the first capture
            lua_insert(parser->L, cg_stack_start + 1);
            // Now stack is: sentinel, first_capture, [other_captures...]
            // Remove any extra captures (keep only sentinel + first capture)
            lua_settop(parser->L, cg_stack_start + 2);
          } else {
            // No captures - capture the matched text span
            size_t capture_len = parser->pos - start_pos;
            // Push sentinel (identifies this as named capture "maybe")
            lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_maybe);
            // Push captured value
            lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
          }
        }
      }
      if (parser->success) {
        { // Capture Match Back "maybe"
          parser->success = false;

          // Search backward through stack for sentinel "maybe"
          for (int i = lua_gettop(parser->L); i >= 1; i--) {
            if (lua_islightuserdata(parser->L, i)) {
              void *ptr = lua_touserdata(parser->L, i);
              if (ptr == (void *)__cg_sentinel_maybe) {
                // Found sentinel - value is at i+1 (use only if it's already a string)
                if (i + 1 <= lua_gettop(parser->L) && lua_type(parser->L, i + 1) == LUA_TSTRING) {
                  size_t match_len;
                  const char *match_str = lua_tolstring(parser->L, i + 1, &match_len);
                  // Try to match at current position
                  if (parser->pos + match_len <= parser->input_len &&
                      memcmp(parser->input + parser->pos, match_str, match_len) == 0) {
                    parser->pos += match_len;
                    parser->success = true;
                  }
                }
                break; // Found sentinel, stop searching regardless of match result
              }
            }
          }
          if (!parser->success) {
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Capture match back 'maybe' failed at position %zu", parser->pos);
#endif
          }
        }
        if (parser->success) {
          { // Capture
            size_t start_pos = parser->pos;
            { // Match literal "done"
              if (parser->pos + 4 <= parser->input_len &&
                  memcmp(parser->input + parser->pos, "done", 4) == 0) {
                parser->pos += 4;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Expected `"
                                               "done"
                                               "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
                PGEN_RECORD_FURTHEST(parser);
              }
            }

            if (parser->success) {
              size_t capture_length = parser->pos - start_pos;
              pgen_checkstack(parser, 1);
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

      pgen_checkstack(parser, 3); // table + two temporaries during fill
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "empty_match", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "empty_match", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_lua_long_string(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "lua_long_string", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 7 patterns
      REMEMBER_POSITION(parser, pos);

      { // Match single character "["
        if (parser->pos < parser->input_len &&
            parser->input[parser->pos] == 91) {
          parser->pos++;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected character `"
                                         "["
                                         "` at position %zu",
                  parser->pos);
#endif
          parser->success = false;
        }
      }
      if (parser->success) {
        { // Capture Group "eq"
          int cg_stack_start = lua_gettop(parser->L);
          size_t start_pos = parser->pos;
          { // Zero or more repetitions
            while (true) {
              { // Match single character "="
                if (parser->pos < parser->input_len &&
                    parser->input[parser->pos] == 61) {
                  parser->pos++;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected character `"
                                                 "="
                                                 "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
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
            int cg_stack_end = lua_gettop(parser->L);
            int captures_produced = cg_stack_end - cg_stack_start;

            pgen_checkstack(parser, 2); // sentinel + value
            if (captures_produced > 0) {
              // Inner pattern produced captures - use the first one
              // Push sentinel (identifies this as named capture "eq")
              lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_eq);
              // Move sentinel before the first capture
              lua_insert(parser->L, cg_stack_start + 1);
              // Now stack is: sentinel, first_capture, [other_captures...]
              // Remove any extra captures (keep only sentinel + first capture)
              lua_settop(parser->L, cg_stack_start + 2);
            } else {
              // No captures - capture the matched text span
              size_t capture_len = parser->pos - start_pos;
              // Push sentinel (identifies this as named capture "eq")
              lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_eq);
              // Push captured value
              lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
            }
          }
        }
        if (parser->success) {
          { // Match single character "["
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 91) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected character `"
                                             "["
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }
          if (parser->success) {
            { // Capture
              size_t start_pos = parser->pos;
              { // Zero or more repetitions
                while (true) {
                  { // Sequence with 2 patterns
                    REMEMBER_INPUT_POSITION(parser, pos);

                    { // Negate (only match if pattern fails)
                      REMEMBER_INPUT_POSITION(parser, pos);

                      { // Sequence with 3 patterns
                        REMEMBER_INPUT_POSITION(parser, pos);

                        { // Match single character "]"
                          if (parser->pos < parser->input_len &&
                              parser->input[parser->pos] == 93) {
                            parser->pos++;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message, "Expected character `"
                                                           "]"
                                                           "` at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                          }
                        }
                        if (parser->success) {
                          { // Capture Match Back "eq"
                            parser->success = false;

                            // Search backward through stack for sentinel "eq"
                            for (int i = lua_gettop(parser->L); i >= 1; i--) {
                              if (lua_islightuserdata(parser->L, i)) {
                                void *ptr = lua_touserdata(parser->L, i);
                                if (ptr == (void *)__cg_sentinel_eq) {
                                  // Found sentinel - value is at i+1 (use only if it's already a string)
                                  if (i + 1 <= lua_gettop(parser->L) && lua_type(parser->L, i + 1) == LUA_TSTRING) {
                                    size_t match_len;
                                    const char *match_str = lua_tolstring(parser->L, i + 1, &match_len);
                                    // Try to match at current position
                                    if (parser->pos + match_len <= parser->input_len &&
                                        memcmp(parser->input + parser->pos, match_str, match_len) == 0) {
                                      parser->pos += match_len;
                                      parser->success = true;
                                    }
                                  }
                                  break; // Found sentinel, stop searching regardless of match result
                                }
                              }
                            }
                            if (!parser->success) {
                              PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
                              sprintf(parser->error_message, "Capture match back 'eq' failed at position %zu", parser->pos);
#endif
                            }
                          }
                          if (parser->success) {
                            { // Match single character "]"
                              if (parser->pos < parser->input_len &&
                                  parser->input[parser->pos] == 93) {
                                parser->pos++;
                              } else {
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message, "Expected character `"
                                                               "]"
                                                               "` at position %zu",
                                        parser->pos);
#endif
                                parser->success = false;
                              }
                            }
                          }
                          if (!parser->success) {
                            RESTORE_INPUT_POSITION(parser, pos);
                          }
                        }
                      }

                      if (parser->success) {
                        // Pattern matched, so negate fails
                        RESTORE_INPUT_POSITION(parser, pos);
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
                        RESTORE_INPUT_POSITION(parser, pos); // Restore original position (technically not necessary since failed pattern should make no changes to position)
                      }
                    }
                    if (parser->success) {
                      { // Match any 1 characters
                        if (parser->pos + 1 <= parser->input_len) {
                          parser->pos += 1;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message, "Expected at least 1 more characters at position %zu", parser->pos);
#endif
                          parser->success = false;
                          PGEN_RECORD_FURTHEST(parser);
                        }
                      }
                      if (!parser->success) {
                        RESTORE_INPUT_POSITION(parser, pos);
                      }
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
                size_t capture_length = parser->pos - start_pos;
                pgen_checkstack(parser, 1);
                lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
              }
            }
            if (parser->success) {
              { // Match single character "]"
                if (parser->pos < parser->input_len &&
                    parser->input[parser->pos] == 93) {
                  parser->pos++;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected character `"
                                                 "]"
                                                 "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }
              if (parser->success) {
                { // Capture Match Back "eq"
                  parser->success = false;

                  // Search backward through stack for sentinel "eq"
                  for (int i = lua_gettop(parser->L); i >= 1; i--) {
                    if (lua_islightuserdata(parser->L, i)) {
                      void *ptr = lua_touserdata(parser->L, i);
                      if (ptr == (void *)__cg_sentinel_eq) {
                        // Found sentinel - value is at i+1 (use only if it's already a string)
                        if (i + 1 <= lua_gettop(parser->L) && lua_type(parser->L, i + 1) == LUA_TSTRING) {
                          size_t match_len;
                          const char *match_str = lua_tolstring(parser->L, i + 1, &match_len);
                          // Try to match at current position
                          if (parser->pos + match_len <= parser->input_len &&
                              memcmp(parser->input + parser->pos, match_str, match_len) == 0) {
                            parser->pos += match_len;
                            parser->success = true;
                          }
                        }
                        break; // Found sentinel, stop searching regardless of match result
                      }
                    }
                  }
                  if (!parser->success) {
                    PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Capture match back 'eq' failed at position %zu", parser->pos);
#endif
                  }
                }
                if (parser->success) {
                  { // Match single character "]"
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 93) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message, "Expected character `"
                                                     "]"
                                                     "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                }
              }
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

      pgen_checkstack(parser, 3); // table + two temporaries during fill
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "lua_long_string", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "lua_long_string", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_mismatch(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "mismatch", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Capture Group "x"
      int cg_stack_start = lua_gettop(parser->L);
      size_t start_pos = parser->pos;
      { // Match literal "abc"
        if (parser->pos + 3 <= parser->input_len &&
            memcmp(parser->input + parser->pos, "abc", 3) == 0) {
          parser->pos += 3;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected `"
                                         "abc"
                                         "` at position %zu",
                  parser->pos);
#endif
          parser->success = false;
          PGEN_RECORD_FURTHEST(parser);
        }
      }

      if (parser->success) {
        int cg_stack_end = lua_gettop(parser->L);
        int captures_produced = cg_stack_end - cg_stack_start;

        pgen_checkstack(parser, 2); // sentinel + value
        if (captures_produced > 0) {
          // Inner pattern produced captures - use the first one
          // Push sentinel (identifies this as named capture "x")
          lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_x);
          // Move sentinel before the first capture
          lua_insert(parser->L, cg_stack_start + 1);
          // Now stack is: sentinel, first_capture, [other_captures...]
          // Remove any extra captures (keep only sentinel + first capture)
          lua_settop(parser->L, cg_stack_start + 2);
        } else {
          // No captures - capture the matched text span
          size_t capture_len = parser->pos - start_pos;
          // Push sentinel (identifies this as named capture "x")
          lua_pushlightuserdata(parser->L, (void *)__cg_sentinel_x);
          // Push captured value
          lua_pushlstring(parser->L, parser->input + start_pos, capture_len);
        }
      }
    }
    if (parser->success) {
      { // Capture Match Back "x"
        parser->success = false;

        // Search backward through stack for sentinel "x"
        for (int i = lua_gettop(parser->L); i >= 1; i--) {
          if (lua_islightuserdata(parser->L, i)) {
            void *ptr = lua_touserdata(parser->L, i);
            if (ptr == (void *)__cg_sentinel_x) {
              // Found sentinel - value is at i+1 (use only if it's already a string)
              if (i + 1 <= lua_gettop(parser->L) && lua_type(parser->L, i + 1) == LUA_TSTRING) {
                size_t match_len;
                const char *match_str = lua_tolstring(parser->L, i + 1, &match_len);
                // Try to match at current position
                if (parser->pos + match_len <= parser->input_len &&
                    memcmp(parser->input + parser->pos, match_str, match_len) == 0) {
                  parser->pos += match_len;
                  parser->success = true;
                }
              }
              break; // Found sentinel, stop searching regardless of match result
            }
          }
        }
        if (!parser->success) {
          PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Capture match back 'x' failed at position %zu", parser->pos);
#endif
        }
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "mismatch", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "mismatch", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

// Initialize parser
static Parser *cmb_init(const char *input, lua_State *L) {
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
  parser->furthest_fail = 0;
  parser->L = L;
  return parser;
}

// Free parser
static void cmb_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_cmb_parse(lua_State *L) {
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
  Parser *parser = cmb_init(input, L);
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
      cmb_free(parser);
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
      cmb_free(parser);
      return 3;
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
    cmb_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  cmb_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg cmb_module[] = {
    {"parse", l_cmb_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}            // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_cmb(lua_State *L) {

  luaL_newlib(L, cmb_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register. Register into a fresh table rather than a
// named global: a name would be shared through package.loaded, so loading
// two parsers compiled with the same parser_name in one process would
// silently overwrite the first module's parse function.
int luaopen_cmb(lua_State *L) {

  lua_newtable(L);
  luaL_register(L, NULL, cmb_module);
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o cmb.so -fPIC cmb.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local cmb = require "cmb"
local result = cmb.parse("your input string")
*/

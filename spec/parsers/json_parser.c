#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// json_parser - generated parser

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

#define REMEMBER_POSITION(parser, pp) \
  ParserPosition pp;                  \
  (pp).pos = (parser)->pos;           \
  (pp).stack_size = lua_gettop((parser)->L);

// Restore parser position
#define RESTORE_POSITION(parser, pp) \
  (parser)->pos = (pp).pos;          \
  lua_settop((parser)->L, (pp).stack_size);

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

// No Cg sentinels defined - stub function
static bool is_cg_sentinel(void *ptr) {
  (void)ptr; // unused
  return false;
}

// Forward declarations
static bool parse_json(Parser *parser);
static bool parse_array(Parser *parser);
static bool parse_char(Parser *parser);
static bool parse_escape(Parser *parser);
static bool parse_false(Parser *parser);
static bool parse_hex(Parser *parser);
static bool parse_member(Parser *parser);
static bool parse_null(Parser *parser);
static bool parse_number(Parser *parser);
static bool parse_object(Parser *parser);
static bool parse_string(Parser *parser);
static bool parse_true(Parser *parser);
static bool parse_unicode(Parser *parser);
static bool parse_value(Parser *parser);
static bool parse_ws(Parser *parser);

// Rule functions
static bool parse_json(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "json", start);
#endif

  { // Sequence with 4 patterns
    REMEMBER_POSITION(parser, pos);

    parse_ws(parser);
    if (parser->success) {
      { // Capture Table
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 2 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all given values
            pgen_checkstack(parser, 1);
            lua_pushlstring(parser->L, "json", 4);
          }
          if (parser->success) {
            { // Choice
              parse_value(parser);

              // Only try alternative if ordinary failure (not labeled failure from T())
              if (!parser->success && !parser->throw_label) {
                parser->success = true;
                { // Throw labeled failure: expected_value
                  parser->success = false;
                  parser->throw_label = "expected_value";
                  parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "expected_value"
                                                 " at position %zu",
                          parser->pos + 1);
#endif
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
      if (parser->success) {
        parse_ws(parser);
        if (parser->success) {
          {   // Choice
            { // Negate (only match if pattern fails)
              REMEMBER_POSITION(parser, pos);

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

              if (parser->success) {
                // Pattern matched, so negate fails
                RESTORE_POSITION(parser, pos);
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
                RESTORE_POSITION(parser, pos); // Restore original position (technically not necessary since failed pattern should make no changes to position)
              }
            }

            // Only try alternative if ordinary failure (not labeled failure from T())
            if (!parser->success && !parser->throw_label) {
              parser->success = true;
              { // Throw labeled failure: expected_eof
                parser->success = false;
                parser->throw_label = "expected_eof";
                parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "expected_eof"
                                               " at position %zu",
                        parser->pos + 1);
#endif
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "json", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "json", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_array(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "array", start);
#endif

  { // Sequence with 5 patterns
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
      parse_ws(parser);
      if (parser->success) {
        { // Capture Table
          int initial_stack_size = lua_gettop(parser->L);
          { // Sequence with 2 patterns
            REMEMBER_POSITION(parser, pos);

            { // Constant Capture
              // A constant capture matches the empty string and produces all given values
              pgen_checkstack(parser, 1);
              lua_pushlstring(parser->L, "array", 5);
            }
            if (parser->success) {
              { // At most 1 repetitions
                size_t rep_count = 0;

                while (rep_count < 1) {
                  size_t before_pos = parser->pos;

                  {
                    { // Sequence with 2 patterns
                      REMEMBER_POSITION(parser, pos);

                      parse_value(parser);
                      if (parser->success) {
                        { // Zero or more repetitions
                          while (true) {
                            { // Sequence with 4 patterns
                              REMEMBER_POSITION(parser, pos);

                              parse_ws(parser);
                              if (parser->success) {
                                { // Match single character ","
                                  if (parser->pos < parser->input_len &&
                                      parser->input[parser->pos] == 44) {
                                    parser->pos++;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message, "Expected character `"
                                                                   ","
                                                                   "` at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                                if (parser->success) {
                                  parse_ws(parser);
                                  if (parser->success) {
                                    { // Choice
                                      parse_value(parser);

                                      // Only try alternative if ordinary failure (not labeled failure from T())
                                      if (!parser->success && !parser->throw_label) {
                                        parser->success = true;
                                        { // Throw labeled failure: expected_array_element
                                          parser->success = false;
                                          parser->throw_label = "expected_array_element";
                                          parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message, "expected_array_element"
                                                                         " at position %zu",
                                                  parser->pos + 1);
#endif
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
                            if (!parser->success) {
                              break;
                            }
                          }
                          // Only recover from ordinary failure, not labeled failure from T()
                          if (!parser->throw_label) {
                            parser->success = true;
                          }
                        }
                        if (!parser->success) {
                          RESTORE_POSITION(parser, pos);
                        }
                      }
                    }
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
        if (parser->success) {
          parse_ws(parser);
          if (parser->success) {
            {   // Choice
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

              // Only try alternative if ordinary failure (not labeled failure from T())
              if (!parser->success && !parser->throw_label) {
                parser->success = true;
                { // Throw labeled failure: expected_closing_bracket
                  parser->success = false;
                  parser->throw_label = "expected_closing_bracket";
                  parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "expected_closing_bracket"
                                                 " at position %zu",
                          parser->pos + 1);
#endif
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "array", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "array", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_char(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "char", start);
#endif

  { // Choice
    parse_escape(parser);

    // Only try alternative if ordinary failure (not labeled failure from T())
    if (!parser->success && !parser->throw_label) {
      parser->success = true;
      { // Sequence with 2 patterns
        REMEMBER_POSITION(parser, pos);

        { // Negate (only match if pattern fails)
          REMEMBER_POSITION(parser, pos);

          { // Match character set "\"\\"
            if (parser->pos < parser->input_len) {
              switch (parser->input[parser->pos]) {
              case 34: /* "\"" */
              case 92: /* "\\" */
                parser->pos++;
                break;
              default:
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Expected one of "
                                               "\"\\\"\\\\\""
                                               " at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected one of "
                                             "\"\\\"\\\\\""
                                             " at position %zu but reached end of input",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          if (parser->success) {
            // Pattern matched, so negate fails
            RESTORE_POSITION(parser, pos);
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
            RESTORE_POSITION(parser, pos); // Restore original position (technically not necessary since failed pattern should make no changes to position)
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
            RESTORE_POSITION(parser, pos);
          }
        }
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "char", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "char", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_escape(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "escape", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match single character "\\"
      if (parser->pos < parser->input_len &&
          parser->input[parser->pos] == 92) {
        parser->pos++;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected character `"
                                       "\\\\"
                                       "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
      }
    }
    if (parser->success) {
      {     // Choice
        {   // Choice
          { // Match character set "\"\\\/bfnrt"
            if (parser->pos < parser->input_len) {
              switch (parser->input[parser->pos]) {
              case 34:  /* "\"" */
              case 92:  /* "\\" */
              case 47:  /* "/" */
              case 98:  /* "b" */
              case 102: /* "f" */
              case 110: /* "n" */
              case 114: /* "r" */
              case 116: /* "t" */
                parser->pos++;
                break;
              default:
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Expected one of "
                                               "\"\\\"\\\\/bfnrt\""
                                               " at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected one of "
                                             "\"\\\"\\\\/bfnrt\""
                                             " at position %zu but reached end of input",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          // Only try alternative if ordinary failure (not labeled failure from T())
          if (!parser->success && !parser->throw_label) {
            parser->success = true;
            parse_unicode(parser);
          }
        }

        // Only try alternative if ordinary failure (not labeled failure from T())
        if (!parser->success && !parser->throw_label) {
          parser->success = true;
          { // Throw labeled failure: invalid_escape_sequence
            parser->success = false;
            parser->throw_label = "invalid_escape_sequence";
            parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "invalid_escape_sequence"
                                           " at position %zu",
                    parser->pos + 1);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "escape", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "escape", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_false(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "false", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match literal "false"
      if (parser->pos + 5 <= parser->input_len &&
          memcmp(parser->input + parser->pos, "false", 5) == 0) {
        parser->pos += 5;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected `"
                                       "false"
                                       "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
        PGEN_RECORD_FURTHEST(parser);
      }
    }
    if (parser->success) {
      { // Capture Table (array-only)
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 2 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all given values
            pgen_checkstack(parser, 1);
            lua_pushlstring(parser->L, "boolean", 7);
          }
          if (parser->success) {
            { // Constant Capture
              // A constant capture matches the empty string and produces all given values
              pgen_checkstack(parser, 1);
              lua_pushboolean(parser->L, 0);
            }
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
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
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "false", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "false", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_hex(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "hex", start);
#endif

  { // Match character range: "09,af,AF"
    if (parser->pos < parser->input_len &&
        ((parser->input[parser->pos] >= 48 && parser->input[parser->pos] <= 57) || (parser->input[parser->pos] >= 97 && parser->input[parser->pos] <= 102) || (parser->input[parser->pos] >= 65 && parser->input[parser->pos] <= 70))) {
      parser->pos++;
    } else {
#ifdef PGEN_ERRORS
      sprintf(parser->error_message, "Expected character in ranges ["
                                     "0"
                                     " - "
                                     "9"
                                     ", "
                                     "a"
                                     " - "
                                     "f"
                                     ", "
                                     "A"
                                     " - "
                                     "F"
                                     "] at position %zu",
              parser->pos);
#endif
      parser->success = false;
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "hex", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "hex", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_member(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "member", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 6 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_checkstack(parser, 1);
        lua_pushlstring(parser->L, "member", 6);
      }
      if (parser->success) {
        parse_string(parser);
        if (parser->success) {
          parse_ws(parser);
          if (parser->success) {
            {   // Choice
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

              // Only try alternative if ordinary failure (not labeled failure from T())
              if (!parser->success && !parser->throw_label) {
                parser->success = true;
                { // Throw labeled failure: expected_colon
                  parser->success = false;
                  parser->throw_label = "expected_colon";
                  parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "expected_colon"
                                                 " at position %zu",
                          parser->pos + 1);
#endif
                }
              }
            }
            if (parser->success) {
              parse_ws(parser);
              if (parser->success) {
                { // Choice
                  parse_value(parser);

                  // Only try alternative if ordinary failure (not labeled failure from T())
                  if (!parser->success && !parser->throw_label) {
                    parser->success = true;
                    { // Throw labeled failure: expected_value
                      parser->success = false;
                      parser->throw_label = "expected_value";
                      parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message, "expected_value"
                                                     " at position %zu",
                              parser->pos + 1);
#endif
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "member", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "member", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_null(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "null", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match literal "null"
      if (parser->pos + 4 <= parser->input_len &&
          memcmp(parser->input + parser->pos, "null", 4) == 0) {
        parser->pos += 4;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected `"
                                       "null"
                                       "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
        PGEN_RECORD_FURTHEST(parser);
      }
    }
    if (parser->success) {
      { // Capture Table (array-only)
        int initial_stack_size = lua_gettop(parser->L);
        { // Constant Capture
          // A constant capture matches the empty string and produces all given values
          pgen_checkstack(parser, 1);
          lua_pushlstring(parser->L, "null", 4);
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
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "null", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "null", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_number(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "number", start);
#endif

  { // Capture Table (array-only)
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_checkstack(parser, 1);
        lua_pushlstring(parser->L, "number", 6);
      }
      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          { // Sequence with 4 patterns
            REMEMBER_POSITION(parser, pos);

            { // At most 1 repetitions
              size_t rep_count = 0;

              while (rep_count < 1) {
                size_t before_pos = parser->pos;

                {
                  { // Match single character "-"
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 45) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message, "Expected character `"
                                                     "-"
                                                     "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
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
            }
            if (parser->success) {
              {   // Choice
                { // Match single character "0"
                  if (parser->pos < parser->input_len &&
                      parser->input[parser->pos] == 48) {
                    parser->pos++;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Expected character `"
                                                   "0"
                                                   "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }

                // Only try alternative if ordinary failure (not labeled failure from T())
                if (!parser->success && !parser->throw_label) {
                  parser->success = true;
                  { // Sequence with 2 patterns
                    REMEMBER_POSITION(parser, pos);

                    { // Match character range: "19"
                      if (parser->pos < parser->input_len &&
                          ((parser->input[parser->pos] >= 49 && parser->input[parser->pos] <= 57))) {
                        parser->pos++;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message, "Expected character in ranges ["
                                                       "1"
                                                       " - "
                                                       "9"
                                                       "] at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                    if (parser->success) {
                      { // Zero or more repetitions
                        while (true) {
                          { // Match character range: "09"
                            if (parser->pos < parser->input_len &&
                                ((parser->input[parser->pos] >= 48 && parser->input[parser->pos] <= 57))) {
                              parser->pos++;
                            } else {
#ifdef PGEN_ERRORS
                              sprintf(parser->error_message, "Expected character in ranges ["
                                                             "0"
                                                             " - "
                                                             "9"
                                                             "] at position %zu",
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
                      if (!parser->success) {
                        RESTORE_POSITION(parser, pos);
                      }
                    }
                  }
                }
              }
              if (parser->success) {
                { // At most 1 repetitions
                  size_t rep_count = 0;

                  while (rep_count < 1) {
                    size_t before_pos = parser->pos;

                    {
                      { // Sequence with 2 patterns
                        REMEMBER_POSITION(parser, pos);

                        { // Match single character "."
                          if (parser->pos < parser->input_len &&
                              parser->input[parser->pos] == 46) {
                            parser->pos++;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message, "Expected character `"
                                                           "."
                                                           "` at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                          }
                        }
                        if (parser->success) {
                          { // At least 1 repetitions
                            REMEMBER_POSITION(parser, pos);
                            size_t rep_count = 0;

                            while (true) {
                              { // Match character range: "09"
                                if (parser->pos < parser->input_len &&
                                    ((parser->input[parser->pos] >= 48 && parser->input[parser->pos] <= 57))) {
                                  parser->pos++;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message, "Expected character in ranges ["
                                                                 "0"
                                                                 " - "
                                                                 "9"
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

                            // Don't recover if labeled failure was thrown
                            if (parser->throw_label) {
                              // Keep failure state, propagate labeled failure
                            } else if (rep_count >= 1) {
                              parser->success = true;
                            } else {
                              RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
                              sprintf(parser->error_message, "Expected 1 repetitions at position %zu", parser->pos);
#endif
                            }
                          }
                          if (!parser->success) {
                            RESTORE_POSITION(parser, pos);
                          }
                        }
                      }
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
                }
                if (parser->success) {
                  { // At most 1 repetitions
                    size_t rep_count = 0;

                    while (rep_count < 1) {
                      size_t before_pos = parser->pos;

                      {
                        { // Sequence with 3 patterns
                          REMEMBER_POSITION(parser, pos);

                          { // Match character set "eE"
                            if (parser->pos < parser->input_len) {
                              switch (parser->input[parser->pos]) {
                              case 101: /* "e" */
                              case 69:  /* "E" */
                                parser->pos++;
                                break;
                              default:
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message, "Expected one of "
                                                               "\"eE\""
                                                               " at position %zu",
                                        parser->pos);
#endif
                                parser->success = false;
                              }
                            } else {
#ifdef PGEN_ERRORS
                              sprintf(parser->error_message, "Expected one of "
                                                             "\"eE\""
                                                             " at position %zu but reached end of input",
                                      parser->pos);
#endif
                              parser->success = false;
                            }
                          }
                          if (parser->success) {
                            { // At most 1 repetitions
                              size_t rep_count = 0;

                              while (rep_count < 1) {
                                size_t before_pos = parser->pos;

                                {
                                  { // Match character set "+-"
                                    if (parser->pos < parser->input_len) {
                                      switch (parser->input[parser->pos]) {
                                      case 43: /* "+" */
                                      case 45: /* "-" */
                                        parser->pos++;
                                        break;
                                      default:
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message, "Expected one of "
                                                                       "\"+-\""
                                                                       " at position %zu",
                                                parser->pos);
#endif
                                        parser->success = false;
                                      }
                                    } else {
#ifdef PGEN_ERRORS
                                      sprintf(parser->error_message, "Expected one of "
                                                                     "\"+-\""
                                                                     " at position %zu but reached end of input",
                                              parser->pos);
#endif
                                      parser->success = false;
                                    }
                                  }
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
                            }
                            if (parser->success) {
                              { // At least 1 repetitions
                                REMEMBER_POSITION(parser, pos);
                                size_t rep_count = 0;

                                while (true) {
                                  { // Match character range: "09"
                                    if (parser->pos < parser->input_len &&
                                        ((parser->input[parser->pos] >= 48 && parser->input[parser->pos] <= 57))) {
                                      parser->pos++;
                                    } else {
#ifdef PGEN_ERRORS
                                      sprintf(parser->error_message, "Expected character in ranges ["
                                                                     "0"
                                                                     " - "
                                                                     "9"
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

                                // Don't recover if labeled failure was thrown
                                if (parser->throw_label) {
                                  // Keep failure state, propagate labeled failure
                                } else if (rep_count >= 1) {
                                  parser->success = true;
                                } else {
                                  RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message, "Expected 1 repetitions at position %zu", parser->pos);
#endif
                                }
                              }
                            }
                            if (!parser->success) {
                              RESTORE_POSITION(parser, pos);
                            }
                          }
                        }
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
                  }
                }
              }
              if (!parser->success) {
                RESTORE_POSITION(parser, pos);
              }
            }
          }

          if (parser->success) {
            size_t capture_length = parser->pos - start_pos;
            pgen_checkstack(parser, 1);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "number", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "number", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_object(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "object", start);
#endif

  { // Sequence with 5 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match single character "{"
      if (parser->pos < parser->input_len &&
          parser->input[parser->pos] == 123) {
        parser->pos++;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected character `"
                                       "{"
                                       "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
      }
    }
    if (parser->success) {
      parse_ws(parser);
      if (parser->success) {
        { // Capture Table
          int initial_stack_size = lua_gettop(parser->L);
          { // Sequence with 2 patterns
            REMEMBER_POSITION(parser, pos);

            { // Constant Capture
              // A constant capture matches the empty string and produces all given values
              pgen_checkstack(parser, 1);
              lua_pushlstring(parser->L, "object", 6);
            }
            if (parser->success) {
              { // At most 1 repetitions
                size_t rep_count = 0;

                while (rep_count < 1) {
                  size_t before_pos = parser->pos;

                  {
                    { // Sequence with 2 patterns
                      REMEMBER_POSITION(parser, pos);

                      parse_member(parser);
                      if (parser->success) {
                        { // Zero or more repetitions
                          while (true) {
                            { // Sequence with 4 patterns
                              REMEMBER_POSITION(parser, pos);

                              parse_ws(parser);
                              if (parser->success) {
                                { // Match single character ","
                                  if (parser->pos < parser->input_len &&
                                      parser->input[parser->pos] == 44) {
                                    parser->pos++;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message, "Expected character `"
                                                                   ","
                                                                   "` at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                                if (parser->success) {
                                  parse_ws(parser);
                                  if (parser->success) {
                                    { // Choice
                                      parse_member(parser);

                                      // Only try alternative if ordinary failure (not labeled failure from T())
                                      if (!parser->success && !parser->throw_label) {
                                        parser->success = true;
                                        { // Throw labeled failure: expected_member
                                          parser->success = false;
                                          parser->throw_label = "expected_member";
                                          parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message, "expected_member"
                                                                         " at position %zu",
                                                  parser->pos + 1);
#endif
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
                            if (!parser->success) {
                              break;
                            }
                          }
                          // Only recover from ordinary failure, not labeled failure from T()
                          if (!parser->throw_label) {
                            parser->success = true;
                          }
                        }
                        if (!parser->success) {
                          RESTORE_POSITION(parser, pos);
                        }
                      }
                    }
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
        if (parser->success) {
          parse_ws(parser);
          if (parser->success) {
            {   // Choice
              { // Match single character "}"
                if (parser->pos < parser->input_len &&
                    parser->input[parser->pos] == 125) {
                  parser->pos++;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected character `"
                                                 "}"
                                                 "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }

              // Only try alternative if ordinary failure (not labeled failure from T())
              if (!parser->success && !parser->throw_label) {
                parser->success = true;
                { // Throw labeled failure: expected_closing_brace
                  parser->success = false;
                  parser->throw_label = "expected_closing_brace";
                  parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "expected_closing_brace"
                                                 " at position %zu",
                          parser->pos + 1);
#endif
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "object", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "object", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_string(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "string", start);
#endif

  { // Sequence with 3 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match single character "\""
      if (parser->pos < parser->input_len &&
          parser->input[parser->pos] == 34) {
        parser->pos++;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected character `"
                                       "\\\""
                                       "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
      }
    }
    if (parser->success) {
      { // Capture Table (array-only)
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 2 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all given values
            pgen_checkstack(parser, 1);
            lua_pushlstring(parser->L, "string", 6);
          }
          if (parser->success) {
            { // Capture
              size_t start_pos = parser->pos;
              { // Zero or more repetitions
                while (true) {
                  parse_char(parser);
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
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
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
      if (parser->success) {
        {   // Choice
          { // Match single character "\""
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 34) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected character `"
                                             "\\\""
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          // Only try alternative if ordinary failure (not labeled failure from T())
          if (!parser->success && !parser->throw_label) {
            parser->success = true;
            { // Throw labeled failure: expected_closing_quote
              parser->success = false;
              parser->throw_label = "expected_closing_quote";
              parser->throw_pos = parser->pos;
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "expected_closing_quote"
                                             " at position %zu",
                      parser->pos + 1);
#endif
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "string", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "string", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_true(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "true", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match literal "true"
      if (parser->pos + 4 <= parser->input_len &&
          memcmp(parser->input + parser->pos, "true", 4) == 0) {
        parser->pos += 4;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected `"
                                       "true"
                                       "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
        PGEN_RECORD_FURTHEST(parser);
      }
    }
    if (parser->success) {
      { // Capture Table (array-only)
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 2 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all given values
            pgen_checkstack(parser, 1);
            lua_pushlstring(parser->L, "boolean", 7);
          }
          if (parser->success) {
            { // Constant Capture
              // A constant capture matches the empty string and produces all given values
              pgen_checkstack(parser, 1);
              lua_pushboolean(parser->L, 1);
            }
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
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
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "true", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "true", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_unicode(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "unicode", start);
#endif

  { // Sequence with 5 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match single character "u"
      if (parser->pos < parser->input_len &&
          parser->input[parser->pos] == 117) {
        parser->pos++;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected character `"
                                       "u"
                                       "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
      }
    }
    if (parser->success) {
      parse_hex(parser);
      if (parser->success) {
        parse_hex(parser);
        if (parser->success) {
          parse_hex(parser);
          if (parser->success) {
            parse_hex(parser);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "unicode", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "unicode", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_value(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "value", start);
#endif

  {           // Choice
    {         // Choice
      {       // Choice
        {     // Choice
          {   // Choice
            { // Choice
              parse_object(parser);

              // Only try alternative if ordinary failure (not labeled failure from T())
              if (!parser->success && !parser->throw_label) {
                parser->success = true;
                parse_array(parser);
              }
            }

            // Only try alternative if ordinary failure (not labeled failure from T())
            if (!parser->success && !parser->throw_label) {
              parser->success = true;
              parse_string(parser);
            }
          }

          // Only try alternative if ordinary failure (not labeled failure from T())
          if (!parser->success && !parser->throw_label) {
            parser->success = true;
            parse_number(parser);
          }
        }

        // Only try alternative if ordinary failure (not labeled failure from T())
        if (!parser->success && !parser->throw_label) {
          parser->success = true;
          parse_true(parser);
        }
      }

      // Only try alternative if ordinary failure (not labeled failure from T())
      if (!parser->success && !parser->throw_label) {
        parser->success = true;
        parse_false(parser);
      }
    }

    // Only try alternative if ordinary failure (not labeled failure from T())
    if (!parser->success && !parser->throw_label) {
      parser->success = true;
      parse_null(parser);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "value", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "value", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_ws(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "ws", start);
#endif

  { // Zero or more repetitions
    while (true) {
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
            sprintf(parser->error_message, "Expected one of "
                                           "\" \\t\\n\\r\""
                                           " at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected one of "
                                         "\" \\t\\n\\r\""
                                         " at position %zu but reached end of input",
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "ws", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "ws", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

// Initialize parser
static Parser *json_parser_init(const char *input, lua_State *L) {
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
static void json_parser_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_json_parser_parse(lua_State *L) {
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
  Parser *parser = json_parser_init(input, L);
  if (!parser) {
    lua_pushnil(L);
    lua_pushstring(L, "Parser initialization failed (memory allocation?)");
    return 2;
  }

  int initial_stack_size = lua_gettop(parser->L);

  parse_json(parser);

  int final_stack_size = lua_gettop(parser->L);

  // Return nil and error info on failure
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size && "Unexpected stack size change on parse failure.");
    lua_pushnil(L);
    if (parser->throw_label) {
      // Labeled failure: return nil, label, position
      lua_pushstring(L, parser->throw_label);
      lua_pushinteger(L, parser->throw_pos + 1); // 1-indexed for Lua
      json_parser_free(parser);
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
      json_parser_free(parser);
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
    json_parser_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  json_parser_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg json_parser_module[] = {
    {"parse", l_json_parser_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}                    // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_json_parser(lua_State *L) {

  luaL_newlib(L, json_parser_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register. Register into a fresh table rather than a
// named global: a name would be shared through package.loaded, so loading
// two parsers compiled with the same parser_name in one process would
// silently overwrite the first module's parse function.
int luaopen_json_parser(lua_State *L) {

  lua_newtable(L);
  luaL_register(L, NULL, json_parser_module);
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o json_parser.so -fPIC json_parser.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local json_parser = require "json_parser"
local result = json_parser.parse("your input string")
*/

#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// furthest - generated parser

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
// Match-time capture (Cmt) infrastructure

static const char __cmt_code_0[] = "    local subject, pos, num = ...\n    if tonumber(num) > 100 then\n      return false\n    end\n    return true\n  ";

static int __cmt_refs[1];

// Initialize Cmt callbacks by loading their Lua code
static void __cmt_init(lua_State *L) {
  if (luaL_loadstring(L, __cmt_code_0) != 0) {
    luaL_error(L, "Failed to load Cmt callback 0: %s", lua_tostring(L, -1));
  }
  __cmt_refs[0] = luaL_ref(L, LUA_REGISTRYINDEX);
}

// Forward declarations
static bool parse_test(Parser *parser);
static bool parse_call(Parser *parser);
static bool parse_cmt_case(Parser *parser);
static bool parse_lookahead_case(Parser *parser);
static bool parse_name(Parser *parser);
static bool parse_negate_case(Parser *parser);
static bool parse_number(Parser *parser);
static bool parse_stmt(Parser *parser);
static bool parse_stmts(Parser *parser);
static bool parse_string(Parser *parser);
static bool parse_value(Parser *parser);

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
            parse_stmts(parser);
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
              parse_lookahead_case(parser);
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
            parse_negate_case(parser);
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
          parse_cmt_case(parser);
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

static bool parse_call(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "call", start);
#endif

  { // Capture
    size_t start_pos = parser->pos;
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Match literal "func"
        if (parser->pos + 4 <= parser->input_len &&
            memcmp(parser->input + parser->pos, "func", 4) == 0) {
          parser->pos += 4;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected `"
                                         "func"
                                         "` at position %zu",
                  parser->pos);
#endif
          parser->success = false;
          PGEN_RECORD_FURTHEST(parser);
        }
      }
      if (parser->success) {
        { // Match literal "()"
          if (parser->pos + 2 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "()", 2) == 0) {
            parser->pos += 2;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected `"
                                           "()"
                                           "` at position %zu",
                    parser->pos);
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

    if (parser->success) {
      size_t capture_length = parser->pos - start_pos;
      pgen_checkstack(parser, 1);
      lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "call", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "call", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_cmt_case(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "cmt_case", start);
#endif

  { // Sequence with 3 patterns
    REMEMBER_POSITION(parser, pos);

    { // At least 1 repetitions
      REMEMBER_POSITION(parser, pos);
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
        RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected 1 repetitions at position %zu", parser->pos);
#endif
      }
    }
    if (parser->success) {
      { // Match-time capture (Cmt id=0)
        int cmt_stack_base = lua_gettop(parser->L);
        size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
        size_t cmt_trail_index = parser->trail_len;
#endif

        { // Capture
          size_t start_pos = parser->pos;
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

          if (parser->success) {
            size_t capture_length = parser->pos - start_pos;
            pgen_checkstack(parser, 1);
            lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
          }
        }

        if (parser->success) {
          size_t pos_after_inner = parser->pos;
          int captures_count = lua_gettop(parser->L) - cmt_stack_base;

          // function + subject + position + copies of the captures
          pgen_checkstack(parser, captures_count + 3);

          // Get the callback function from registry
          lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cmt_refs[0]);

          // Push arguments: subject, position (after inner pattern)
          lua_pushlstring(parser->L, parser->input, parser->input_len);
          lua_pushinteger(parser->L, (lua_Integer)(pos_after_inner + 1)); // 1-based

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
            PGEN_RECORD_FURTHEST(parser); // record at pos_after_inner, before rewind
            parser->pos = cmt_start_pos;
          } else {
            int first_type = lua_type(parser->L, cmt_stack_base + 1);

            if (first_type == LUA_TNUMBER) {
              // Number = new position (1-based from Lua)
              lua_Integer new_pos = lua_tointeger(parser->L, cmt_stack_base + 1);
              new_pos--; // Convert to 0-based
              // Per lpeg: must be in range [pos_after_inner, input_len]
              if (new_pos >= (lua_Integer)pos_after_inner && new_pos <= (lua_Integer)parser->input_len) {
                parser->pos = (size_t)new_pos;
                parser->success = true;
              } else {
                parser->success = false;
                PGEN_RECORD_FURTHEST(parser); // record at pos_after_inner, before rewind
                parser->pos = cmt_start_pos;
              }
            } else if (first_type == LUA_TBOOLEAN && lua_toboolean(parser->L, cmt_stack_base + 1)) {
              // true = succeed without consuming (position stays at pos_after_inner)
              parser->success = true;
            } else {
              // false, nil, or other = fail
              parser->success = false;
              PGEN_RECORD_FURTHEST(parser); // record at pos_after_inner, before rewind
              parser->pos = cmt_start_pos;
            }
          }

          // Handle captures: remove first return value, keep extras as captures
          if (parser->success && returns_count > 1) {
            lua_remove(parser->L, cmt_stack_base + 1); // Remove first return value
            // Remaining values are the new captures
          } else {
            lua_settop(parser->L, cmt_stack_base); // Clear all returns
          }

#ifdef PGEN_HAS_IND
          // Callback rejected the match: undo indenter operations performed by the
          // inner pattern (an inner failure rewinds itself)
          if (!parser->success) {
            pgen_ind_trail_rewind(parser, cmt_trail_index);
          }
#endif
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
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "cmt_case", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "cmt_case", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_lookahead_case(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "lookahead_case", start);
#endif

  { // Sequence with 3 patterns
    REMEMBER_POSITION(parser, pos);

    { // Lookahead (match without consuming input)
      REMEMBER_POSITION(parser, pos);

      { // Match literal "ab"
        if (parser->pos + 2 <= parser->input_len &&
            memcmp(parser->input + parser->pos, "ab", 2) == 0) {
          parser->pos += 2;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected `"
                                         "ab"
                                         "` at position %zu",
                  parser->pos);
#endif
          parser->success = false;
          PGEN_RECORD_FURTHEST(parser);
        }
      }

      if (parser->success) {
        // Pattern matched, but we don't consume any input
        RESTORE_POSITION(parser, pos);
      }
    }
    if (parser->success) {
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
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "lookahead_case", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "lookahead_case", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_name(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "name", start);
#endif

  { // Capture
    size_t start_pos = parser->pos;
    { // At least 1 repetitions
      REMEMBER_POSITION(parser, pos);
      size_t rep_count = 0;

      while (true) {
        { // Match character range: "az"
          if (parser->pos < parser->input_len &&
              ((parser->input[parser->pos] >= 97 && parser->input[parser->pos] <= 122))) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected character in ranges ["
                                           "a"
                                           " - "
                                           "z"
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

    if (parser->success) {
      size_t capture_length = parser->pos - start_pos;
      pgen_checkstack(parser, 1);
      lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "name", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "name", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_negate_case(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "negate_case", start);
#endif

  { // Sequence with 4 patterns
    REMEMBER_POSITION(parser, pos);

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
      { // Negate (only match if pattern fails)
        REMEMBER_POSITION(parser, pos);

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
        if (parser->success) {
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
        }
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "negate_case", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "negate_case", parser->pos);
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

  { // Capture
    size_t start_pos = parser->pos;
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

    if (parser->success) {
      size_t capture_length = parser->pos - start_pos;
      pgen_checkstack(parser, 1);
      lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
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

static bool parse_stmt(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "stmt", start);
#endif

  { // Sequence with 6 patterns
    REMEMBER_POSITION(parser, pos);

    { // Zero or more repetitions
      while (true) {
        { // Match character set " \t\n"
          if (parser->pos < parser->input_len) {
            switch (parser->input[parser->pos]) {
            case 32: /* " " */
            case 9:  /* "\t" */
            case 10: /* "\n" */
              parser->pos++;
              break;
            default:
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected one of "
                                             "\" \\t\\n\""
                                             " at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected one of "
                                           "\" \\t\\n\""
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
    if (parser->success) {
      parse_name(parser);
      if (parser->success) {
        { // Zero or more repetitions
          while (true) {
            { // Match character set " \t\n"
              if (parser->pos < parser->input_len) {
                switch (parser->input[parser->pos]) {
                case 32: /* " " */
                case 9:  /* "\t" */
                case 10: /* "\n" */
                  parser->pos++;
                  break;
                default:
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected one of "
                                                 "\" \\t\\n\""
                                                 " at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Expected one of "
                                               "\" \\t\\n\""
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
        if (parser->success) {
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
          if (parser->success) {
            { // Zero or more repetitions
              while (true) {
                { // Match character set " \t\n"
                  if (parser->pos < parser->input_len) {
                    switch (parser->input[parser->pos]) {
                    case 32: /* " " */
                    case 9:  /* "\t" */
                    case 10: /* "\n" */
                      parser->pos++;
                      break;
                    default:
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message, "Expected one of "
                                                     "\" \\t\\n\""
                                                     " at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Expected one of "
                                                   "\" \\t\\n\""
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
            if (parser->success) {
              parse_value(parser);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "stmt", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "stmt", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_stmts(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "stmts", start);
#endif

  { // Sequence with 4 patterns
    REMEMBER_POSITION(parser, pos);

    parse_stmt(parser);
    if (parser->success) {
      { // Zero or more repetitions
        while (true) {
          { // Sequence with 3 patterns
            REMEMBER_POSITION(parser, pos);

            { // Zero or more repetitions
              while (true) {
                { // Match character set " \t\n"
                  if (parser->pos < parser->input_len) {
                    switch (parser->input[parser->pos]) {
                    case 32: /* " " */
                    case 9:  /* "\t" */
                    case 10: /* "\n" */
                      parser->pos++;
                      break;
                    default:
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message, "Expected one of "
                                                     "\" \\t\\n\""
                                                     " at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Expected one of "
                                                   "\" \\t\\n\""
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
            if (parser->success) {
              { // Match single character ";"
                if (parser->pos < parser->input_len &&
                    parser->input[parser->pos] == 59) {
                  parser->pos++;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected character `"
                                                 ";"
                                                 "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }
              if (parser->success) {
                parse_stmt(parser);
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
      if (parser->success) {
        { // Zero or more repetitions
          while (true) {
            { // Match character set " \t\n"
              if (parser->pos < parser->input_len) {
                switch (parser->input[parser->pos]) {
                case 32: /* " " */
                case 9:  /* "\t" */
                case 10: /* "\n" */
                  parser->pos++;
                  break;
                default:
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected one of "
                                                 "\" \\t\\n\""
                                                 " at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Expected one of "
                                               "\" \\t\\n\""
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
        if (parser->success) {
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
        }
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "stmts", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "stmts", parser->pos);
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

    { // Match single character "'"
      if (parser->pos < parser->input_len &&
          parser->input[parser->pos] == 39) {
        parser->pos++;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected character `"
                                       "'"
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
              REMEMBER_POSITION(parser, pos);

              { // Negate (only match if pattern fails)
                REMEMBER_POSITION(parser, pos);

                { // Match single character "'"
                  if (parser->pos < parser->input_len &&
                      parser->input[parser->pos] == 39) {
                    parser->pos++;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Expected character `"
                                                   "'"
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
        { // Match single character "'"
          if (parser->pos < parser->input_len &&
              parser->input[parser->pos] == 39) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected character `"
                                           "'"
                                           "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
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

  {   // Choice
    { // Choice
      parse_number(parser);

      // Only try alternative if ordinary failure (not labeled failure from T())
      if (!parser->success && !parser->throw_label) {
        parser->success = true;
        parse_string(parser);
      }
    }

    // Only try alternative if ordinary failure (not labeled failure from T())
    if (!parser->success && !parser->throw_label) {
      parser->success = true;
      parse_call(parser);
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

// Initialize parser
static Parser *furthest_init(const char *input, lua_State *L) {
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
static void furthest_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_furthest_parse(lua_State *L) {
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
  Parser *parser = furthest_init(input, L);
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
      furthest_free(parser);
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
      furthest_free(parser);
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
    furthest_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  furthest_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg furthest_module[] = {
    {"parse", l_furthest_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}                 // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_furthest(lua_State *L) {
  __cmt_init(L);
  luaL_newlib(L, furthest_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register. Register into a fresh table rather than a
// named global: a name would be shared through package.loaded, so loading
// two parsers compiled with the same parser_name in one process would
// silently overwrite the first module's parse function.
int luaopen_furthest(lua_State *L) {
  __cmt_init(L);
  lua_newtable(L);
  luaL_register(L, NULL, furthest_module);
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o furthest.so -fPIC furthest.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local furthest = require "furthest"
local result = furthest.parse("your input string")
*/

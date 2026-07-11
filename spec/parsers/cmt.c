#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// cmt - generated parser

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

// No Cg sentinels defined - stubs
static int __cg_name_refs[1];

static int cg_sentinel_index(void *ptr) {
  (void)ptr; // unused
  return -1;
}

static bool is_cg_sentinel(void *ptr) {
  (void)ptr; // unused
  return false;
}
// Match-time capture (Cmt) infrastructure

static const char __cmt_code_0[] = "    local subject, pos = ...\n    -- Skip any following spaces\n    while pos <= #subject and subject:sub(pos, pos) == \" \" do\n      pos = pos + 1\n    end\n    return pos\n  ";
static const char __cmt_code_1[] = "    local subject, pos = ...\n    return false  -- fail the match\n  ";
static const char __cmt_code_2[] = "    local subject, pos = ...\n    return pos  -- just return the position after \"hello\"\n  ";
static const char __cmt_code_3[] = "    local subject, pos = ...\n    -- no return statement\n  ";
static const char __cmt_code_4[] = "    local subject, pos = ...\n    return nil  -- fail the match\n  ";
static const char __cmt_code_5[] = "    local subject, pos = ...\n    return true  -- succeed, position stays after \"hello\"\n  ";
static const char __cmt_code_6[] = "    local subject, pos = ...\n    return pos, \"first\", \"second\", 123\n  ";
static const char __cmt_code_7[] = "    local subject, pos, cap1, cap2 = ...\n    if cap1 == \"foo\" and cap2 == \"bar\" then\n      return pos, cap1 .. \"-\" .. cap2  -- combine captures\n    end\n    return false\n  ";
static const char __cmt_code_8[] = "    local subject, pos = ...\n    return pos, \"cap1\", \"cap2\"\n  ";

static int __cmt_refs[9];

// Initialize Cmt callbacks by loading their Lua code
static void __cmt_init(lua_State *L) {
  if (luaL_loadstring(L, __cmt_code_0) != 0) {
    luaL_error(L, "Failed to load Cmt callback 0: %s", lua_tostring(L, -1));
  }
  __cmt_refs[0] = luaL_ref(L, LUA_REGISTRYINDEX);
  if (luaL_loadstring(L, __cmt_code_1) != 0) {
    luaL_error(L, "Failed to load Cmt callback 1: %s", lua_tostring(L, -1));
  }
  __cmt_refs[1] = luaL_ref(L, LUA_REGISTRYINDEX);
  if (luaL_loadstring(L, __cmt_code_2) != 0) {
    luaL_error(L, "Failed to load Cmt callback 2: %s", lua_tostring(L, -1));
  }
  __cmt_refs[2] = luaL_ref(L, LUA_REGISTRYINDEX);
  if (luaL_loadstring(L, __cmt_code_3) != 0) {
    luaL_error(L, "Failed to load Cmt callback 3: %s", lua_tostring(L, -1));
  }
  __cmt_refs[3] = luaL_ref(L, LUA_REGISTRYINDEX);
  if (luaL_loadstring(L, __cmt_code_4) != 0) {
    luaL_error(L, "Failed to load Cmt callback 4: %s", lua_tostring(L, -1));
  }
  __cmt_refs[4] = luaL_ref(L, LUA_REGISTRYINDEX);
  if (luaL_loadstring(L, __cmt_code_5) != 0) {
    luaL_error(L, "Failed to load Cmt callback 5: %s", lua_tostring(L, -1));
  }
  __cmt_refs[5] = luaL_ref(L, LUA_REGISTRYINDEX);
  if (luaL_loadstring(L, __cmt_code_6) != 0) {
    luaL_error(L, "Failed to load Cmt callback 6: %s", lua_tostring(L, -1));
  }
  __cmt_refs[6] = luaL_ref(L, LUA_REGISTRYINDEX);
  if (luaL_loadstring(L, __cmt_code_7) != 0) {
    luaL_error(L, "Failed to load Cmt callback 7: %s", lua_tostring(L, -1));
  }
  __cmt_refs[7] = luaL_ref(L, LUA_REGISTRYINDEX);
  if (luaL_loadstring(L, __cmt_code_8) != 0) {
    luaL_error(L, "Failed to load Cmt callback 8: %s", lua_tostring(L, -1));
  }
  __cmt_refs[8] = luaL_ref(L, LUA_REGISTRYINDEX);
}

// Forward declarations
static bool parse_test(Parser *parser);
static bool parse_captures_passed(Parser *parser);
static bool parse_inside_ct(Parser *parser);
static bool parse_no_return(Parser *parser);
static bool parse_return_extra_captures(Parser *parser);
static bool parse_return_false(Parser *parser);
static bool parse_return_nil(Parser *parser);
static bool parse_return_pos(Parser *parser);
static bool parse_return_true(Parser *parser);
static bool parse_skip_chars(Parser *parser);

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

  {                 // Choice
    {               // Choice
      {             // Choice
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
                        PGEN_RECORD_FURTHEST(parser);
                      }
                    }
                    if (parser->success) {
                      parse_return_pos(parser);
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
                        parse_return_true(parser);
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
                      parse_return_false(parser);
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
                    parse_return_nil(parser);
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
                    PGEN_RECORD_FURTHEST(parser);
                  }
                }
                if (parser->success) {
                  parse_return_extra_captures(parser);
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
                  PGEN_RECORD_FURTHEST(parser);
                }
              }
              if (parser->success) {
                parse_captures_passed(parser);
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

            { // Match literal "7:"
              if (parser->pos + 2 <= parser->input_len &&
                  memcmp(parser->input + parser->pos, "7:", 2) == 0) {
                parser->pos += 2;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Expected `"
                                               "7:"
                                               "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
                PGEN_RECORD_FURTHEST(parser);
              }
            }
            if (parser->success) {
              parse_inside_ct(parser);
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

          { // Match literal "8:"
            if (parser->pos + 2 <= parser->input_len &&
                memcmp(parser->input + parser->pos, "8:", 2) == 0) {
              parser->pos += 2;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected `"
                                             "8:"
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
              PGEN_RECORD_FURTHEST(parser);
            }
          }
          if (parser->success) {
            parse_skip_chars(parser);
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

        { // Match literal "9:"
          if (parser->pos + 2 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "9:", 2) == 0) {
            parser->pos += 2;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected `"
                                           "9:"
                                           "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
            PGEN_RECORD_FURTHEST(parser);
          }
        }
        if (parser->success) {
          parse_no_return(parser);
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

static bool parse_captures_passed(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "captures_passed", start);
#endif

  { // Match-time capture (Cmt id=7)
    int cmt_stack_base = lua_gettop(parser->L);
    size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
    size_t cmt_trail_index = parser->trail_len;
#endif

    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Capture
        size_t start_pos = parser->pos;
        { // Match literal "foo"
          if (parser->pos + 3 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "foo", 3) == 0) {
            parser->pos += 3;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected `"
                                           "foo"
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
      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          { // Match literal "bar"
            if (parser->pos + 3 <= parser->input_len &&
                memcmp(parser->input + parser->pos, "bar", 3) == 0) {
              parser->pos += 3;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected `"
                                             "bar"
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
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      size_t pos_after_inner = parser->pos;
      int captures_count = lua_gettop(parser->L) - cmt_stack_base;

      // function + subject + position + copies of the captures
      pgen_checkstack(parser, captures_count + 3);

      // Get the callback function from registry
      lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cmt_refs[7]);

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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "captures_passed", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "captures_passed", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_inside_ct(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "inside_ct", start);
#endif

  { // Capture Table (array-only)
    int initial_stack_size = lua_gettop(parser->L);
    { // Match-time capture (Cmt id=8)
      int cmt_stack_base = lua_gettop(parser->L);
      size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
      size_t cmt_trail_index = parser->trail_len;
#endif

      { // Match literal "xyz"
        if (parser->pos + 3 <= parser->input_len &&
            memcmp(parser->input + parser->pos, "xyz", 3) == 0) {
          parser->pos += 3;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected `"
                                         "xyz"
                                         "` at position %zu",
                  parser->pos);
#endif
          parser->success = false;
          PGEN_RECORD_FURTHEST(parser);
        }
      }

      if (parser->success) {
        size_t pos_after_inner = parser->pos;
        int captures_count = lua_gettop(parser->L) - cmt_stack_base;

        // function + subject + position + copies of the captures
        pgen_checkstack(parser, captures_count + 3);

        // Get the callback function from registry
        lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cmt_refs[8]);

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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "inside_ct", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "inside_ct", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_no_return(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "no_return", start);
#endif

  { // Match-time capture (Cmt id=3)
    int cmt_stack_base = lua_gettop(parser->L);
    size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
    size_t cmt_trail_index = parser->trail_len;
#endif

    { // Match literal "test"
      if (parser->pos + 4 <= parser->input_len &&
          memcmp(parser->input + parser->pos, "test", 4) == 0) {
        parser->pos += 4;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected `"
                                       "test"
                                       "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
        PGEN_RECORD_FURTHEST(parser);
      }
    }

    if (parser->success) {
      size_t pos_after_inner = parser->pos;
      int captures_count = lua_gettop(parser->L) - cmt_stack_base;

      // function + subject + position + copies of the captures
      pgen_checkstack(parser, captures_count + 3);

      // Get the callback function from registry
      lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cmt_refs[3]);

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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "no_return", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "no_return", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_return_extra_captures(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "return_extra_captures", start);
#endif

  { // Match-time capture (Cmt id=6)
    int cmt_stack_base = lua_gettop(parser->L);
    size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
    size_t cmt_trail_index = parser->trail_len;
#endif

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
      size_t pos_after_inner = parser->pos;
      int captures_count = lua_gettop(parser->L) - cmt_stack_base;

      // function + subject + position + copies of the captures
      pgen_checkstack(parser, captures_count + 3);

      // Get the callback function from registry
      lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cmt_refs[6]);

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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "return_extra_captures", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "return_extra_captures", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_return_false(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "return_false", start);
#endif

  { // Match-time capture (Cmt id=1)
    int cmt_stack_base = lua_gettop(parser->L);
    size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
    size_t cmt_trail_index = parser->trail_len;
#endif

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
        PGEN_RECORD_FURTHEST(parser);
      }
    }

    if (parser->success) {
      size_t pos_after_inner = parser->pos;
      int captures_count = lua_gettop(parser->L) - cmt_stack_base;

      // function + subject + position + copies of the captures
      pgen_checkstack(parser, captures_count + 3);

      // Get the callback function from registry
      lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cmt_refs[1]);

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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "return_false", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "return_false", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_return_nil(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "return_nil", start);
#endif

  { // Match-time capture (Cmt id=4)
    int cmt_stack_base = lua_gettop(parser->L);
    size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
    size_t cmt_trail_index = parser->trail_len;
#endif

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
        PGEN_RECORD_FURTHEST(parser);
      }
    }

    if (parser->success) {
      size_t pos_after_inner = parser->pos;
      int captures_count = lua_gettop(parser->L) - cmt_stack_base;

      // function + subject + position + copies of the captures
      pgen_checkstack(parser, captures_count + 3);

      // Get the callback function from registry
      lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cmt_refs[4]);

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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "return_nil", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "return_nil", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_return_pos(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "return_pos", start);
#endif

  { // Match-time capture (Cmt id=2)
    int cmt_stack_base = lua_gettop(parser->L);
    size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
    size_t cmt_trail_index = parser->trail_len;
#endif

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
        PGEN_RECORD_FURTHEST(parser);
      }
    }

    if (parser->success) {
      size_t pos_after_inner = parser->pos;
      int captures_count = lua_gettop(parser->L) - cmt_stack_base;

      // function + subject + position + copies of the captures
      pgen_checkstack(parser, captures_count + 3);

      // Get the callback function from registry
      lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cmt_refs[2]);

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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "return_pos", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "return_pos", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_return_true(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "return_true", start);
#endif

  { // Match-time capture (Cmt id=5)
    int cmt_stack_base = lua_gettop(parser->L);
    size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
    size_t cmt_trail_index = parser->trail_len;
#endif

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
        PGEN_RECORD_FURTHEST(parser);
      }
    }

    if (parser->success) {
      size_t pos_after_inner = parser->pos;
      int captures_count = lua_gettop(parser->L) - cmt_stack_base;

      // function + subject + position + copies of the captures
      pgen_checkstack(parser, captures_count + 3);

      // Get the callback function from registry
      lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cmt_refs[5]);

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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "return_true", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "return_true", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_skip_chars(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "skip_chars", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match-time capture (Cmt id=0)
      int cmt_stack_base = lua_gettop(parser->L);
      size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
      size_t cmt_trail_index = parser->trail_len;
#endif

      { // Match literal "start"
        if (parser->pos + 5 <= parser->input_len &&
            memcmp(parser->input + parser->pos, "start", 5) == 0) {
          parser->pos += 5;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected `"
                                         "start"
                                         "` at position %zu",
                  parser->pos);
#endif
          parser->success = false;
          PGEN_RECORD_FURTHEST(parser);
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
      { // Match literal "end"
        if (parser->pos + 3 <= parser->input_len &&
            memcmp(parser->input + parser->pos, "end", 3) == 0) {
          parser->pos += 3;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected `"
                                         "end"
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "skip_chars", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "skip_chars", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

// Initialize parser
static Parser *cmt_init(const char *input, lua_State *L) {
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
static void cmt_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_cmt_parse(lua_State *L) {
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
  Parser *parser = cmt_init(input, L);
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
      cmt_free(parser);
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
      cmt_free(parser);
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
    cmt_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  cmt_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg cmt_module[] = {
    {"parse", l_cmt_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}            // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_cmt(lua_State *L) {

  __cmt_init(L);
  luaL_newlib(L, cmt_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register. Register into a fresh table rather than a
// named global: a name would be shared through package.loaded, so loading
// two parsers compiled with the same parser_name in one process would
// silently overwrite the first module's parse function.
int luaopen_cmt(lua_State *L) {

  __cmt_init(L);
  lua_newtable(L);
  luaL_register(L, NULL, cmt_module);
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o cmt.so -fPIC cmt.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local cmt = require "cmt"
local result = cmt.parse("your input string")
*/

#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// trie_optimization - generated parser

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

// Forward declarations
static bool parse_test(Parser *parser);
static bool parse_test1(Parser *parser);
static bool parse_test2(Parser *parser);
static bool parse_test3(Parser *parser);
static bool parse_test4(Parser *parser);
static bool parse_test5(Parser *parser);

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
              parse_test1(parser);
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
                parse_test2(parser);
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
              parse_test3(parser);
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
            parse_test4(parser);
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
          parse_test5(parser);
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

static bool parse_test1(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test1", start);
#endif

  { // Capture
    size_t start_pos = parser->pos;
    { // Trie match for: "foo", "bar", "baz", "qux"
      REMEMBER_POSITION(parser, trie_start);
      size_t last_terminal_pos = 0;
      int has_terminal = 0;

      if (parser->pos < parser->input_len) {
        switch (parser->input[parser->pos]) {
        case 98: // "b"
          parser->pos++;

          if (parser->pos < parser->input_len) {
            switch (parser->input[parser->pos]) {
            case 97: // "a"
              parser->pos++;

              if (parser->pos < parser->input_len) {
                switch (parser->input[parser->pos]) {
                case 114: // "r"
                  parser->pos++;
                  break;
                case 122: // "z"
                  parser->pos++;
                  break;
                default:
                  parser->success = false;
                  if (has_terminal) {
                    parser->pos = last_terminal_pos;
                    parser->success = true;
                  } else {
                    PGEN_RECORD_FURTHEST(parser);
                  }
                }
              } else {
                parser->success = false;
                if (has_terminal) {
                  parser->pos = last_terminal_pos;
                  parser->success = true;
                } else {
                  PGEN_RECORD_FURTHEST(parser);
                }
              }
              break;
            default:
              parser->success = false;
              if (has_terminal) {
                parser->pos = last_terminal_pos;
                parser->success = true;
              } else {
                PGEN_RECORD_FURTHEST(parser);
              }
            }
          } else {
            parser->success = false;
            if (has_terminal) {
              parser->pos = last_terminal_pos;
              parser->success = true;
            } else {
              PGEN_RECORD_FURTHEST(parser);
            }
          }
          break;
        case 102: // "f"
          parser->pos++;

          if (parser->pos < parser->input_len) {
            switch (parser->input[parser->pos]) {
            case 111: // "o"
              parser->pos++;

              if (parser->pos < parser->input_len) {
                switch (parser->input[parser->pos]) {
                case 111: // "o"
                  parser->pos++;
                  break;
                default:
                  parser->success = false;
                  if (has_terminal) {
                    parser->pos = last_terminal_pos;
                    parser->success = true;
                  } else {
                    PGEN_RECORD_FURTHEST(parser);
                  }
                }
              } else {
                parser->success = false;
                if (has_terminal) {
                  parser->pos = last_terminal_pos;
                  parser->success = true;
                } else {
                  PGEN_RECORD_FURTHEST(parser);
                }
              }
              break;
            default:
              parser->success = false;
              if (has_terminal) {
                parser->pos = last_terminal_pos;
                parser->success = true;
              } else {
                PGEN_RECORD_FURTHEST(parser);
              }
            }
          } else {
            parser->success = false;
            if (has_terminal) {
              parser->pos = last_terminal_pos;
              parser->success = true;
            } else {
              PGEN_RECORD_FURTHEST(parser);
            }
          }
          break;
        case 113: // "q"
          parser->pos++;

          if (parser->pos < parser->input_len) {
            switch (parser->input[parser->pos]) {
            case 117: // "u"
              parser->pos++;

              if (parser->pos < parser->input_len) {
                switch (parser->input[parser->pos]) {
                case 120: // "x"
                  parser->pos++;
                  break;
                default:
                  parser->success = false;
                  if (has_terminal) {
                    parser->pos = last_terminal_pos;
                    parser->success = true;
                  } else {
                    PGEN_RECORD_FURTHEST(parser);
                  }
                }
              } else {
                parser->success = false;
                if (has_terminal) {
                  parser->pos = last_terminal_pos;
                  parser->success = true;
                } else {
                  PGEN_RECORD_FURTHEST(parser);
                }
              }
              break;
            default:
              parser->success = false;
              if (has_terminal) {
                parser->pos = last_terminal_pos;
                parser->success = true;
              } else {
                PGEN_RECORD_FURTHEST(parser);
              }
            }
          } else {
            parser->success = false;
            if (has_terminal) {
              parser->pos = last_terminal_pos;
              parser->success = true;
            } else {
              PGEN_RECORD_FURTHEST(parser);
            }
          }
          break;
        default:
          parser->success = false;
          if (has_terminal) {
            parser->pos = last_terminal_pos;
            parser->success = true;
          } else {
            PGEN_RECORD_FURTHEST(parser);
          }
        }
      } else {
        parser->success = false;
        if (has_terminal) {
          parser->pos = last_terminal_pos;
          parser->success = true;
        } else {
          PGEN_RECORD_FURTHEST(parser);
        }
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, trie_start);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test1", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test1", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_test2(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test2", start);
#endif

  {   // Choice
    { // Capture
      size_t start_pos = parser->pos;
      { // Trie match for: "abd", "abc", "ab"
        REMEMBER_POSITION(parser, trie_start);
        size_t last_terminal_pos = 0;
        int has_terminal = 0;

        if (parser->pos < parser->input_len) {
          switch (parser->input[parser->pos]) {
          case 97: // "a"
            parser->pos++;

            if (parser->pos < parser->input_len) {
              switch (parser->input[parser->pos]) {
              case 98: // "b"
                parser->pos++;
                if (!has_terminal || parser->pos > last_terminal_pos) {
                  last_terminal_pos = parser->pos;
                  has_terminal = 1;
                }
                if (parser->pos < parser->input_len) {
                  switch (parser->input[parser->pos]) {
                  case 99: // "c"
                    parser->pos++;
                    break;
                  case 100: // "d"
                    parser->pos++;
                    break;
                  default:
                    parser->success = false;
                    if (has_terminal) {
                      parser->pos = last_terminal_pos;
                      parser->success = true;
                    } else {
                      PGEN_RECORD_FURTHEST(parser);
                    }
                  }
                } else {
                  parser->success = false;
                  if (has_terminal) {
                    parser->pos = last_terminal_pos;
                    parser->success = true;
                  } else {
                    PGEN_RECORD_FURTHEST(parser);
                  }
                }
                if (!parser->success) {
                  // Partial match is valid: "ab"
                  parser->success = true;
                }
                break;
              default:
                parser->success = false;
                if (has_terminal) {
                  parser->pos = last_terminal_pos;
                  parser->success = true;
                } else {
                  PGEN_RECORD_FURTHEST(parser);
                }
              }
            } else {
              parser->success = false;
              if (has_terminal) {
                parser->pos = last_terminal_pos;
                parser->success = true;
              } else {
                PGEN_RECORD_FURTHEST(parser);
              }
            }
            break;
          default:
            parser->success = false;
            if (has_terminal) {
              parser->pos = last_terminal_pos;
              parser->success = true;
            } else {
              PGEN_RECORD_FURTHEST(parser);
            }
          }
        } else {
          parser->success = false;
          if (has_terminal) {
            parser->pos = last_terminal_pos;
            parser->success = true;
          } else {
            PGEN_RECORD_FURTHEST(parser);
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, trie_start);
        }
      }

      if (parser->success) {
        size_t capture_length = parser->pos - start_pos;
        pgen_checkstack(parser, 1);
        lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
      }
    }

    // Only try alternative if ordinary failure (not labeled failure from T())
    if (!parser->success && !parser->throw_label) {
      parser->success = true;
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
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test2", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test2", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_test3(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test3", start);
#endif

  { // Capture
    size_t start_pos = parser->pos;
    { // Trie match for: "fork", "foo", "for"
      REMEMBER_POSITION(parser, trie_start);
      size_t last_terminal_pos = 0;
      int has_terminal = 0;

      if (parser->pos < parser->input_len) {
        switch (parser->input[parser->pos]) {
        case 102: // "f"
          parser->pos++;

          if (parser->pos < parser->input_len) {
            switch (parser->input[parser->pos]) {
            case 111: // "o"
              parser->pos++;

              if (parser->pos < parser->input_len) {
                switch (parser->input[parser->pos]) {
                case 111: // "o"
                  parser->pos++;
                  break;
                case 114: // "r"
                  parser->pos++;
                  if (!has_terminal || parser->pos > last_terminal_pos) {
                    last_terminal_pos = parser->pos;
                    has_terminal = 1;
                  }
                  if (parser->pos < parser->input_len) {
                    switch (parser->input[parser->pos]) {
                    case 107: // "k"
                      parser->pos++;
                      break;
                    default:
                      parser->success = false;
                      if (has_terminal) {
                        parser->pos = last_terminal_pos;
                        parser->success = true;
                      } else {
                        PGEN_RECORD_FURTHEST(parser);
                      }
                    }
                  } else {
                    parser->success = false;
                    if (has_terminal) {
                      parser->pos = last_terminal_pos;
                      parser->success = true;
                    } else {
                      PGEN_RECORD_FURTHEST(parser);
                    }
                  }
                  if (!parser->success) {
                    // Partial match is valid: "for"
                    parser->success = true;
                  }
                  break;
                default:
                  parser->success = false;
                  if (has_terminal) {
                    parser->pos = last_terminal_pos;
                    parser->success = true;
                  } else {
                    PGEN_RECORD_FURTHEST(parser);
                  }
                }
              } else {
                parser->success = false;
                if (has_terminal) {
                  parser->pos = last_terminal_pos;
                  parser->success = true;
                } else {
                  PGEN_RECORD_FURTHEST(parser);
                }
              }
              break;
            default:
              parser->success = false;
              if (has_terminal) {
                parser->pos = last_terminal_pos;
                parser->success = true;
              } else {
                PGEN_RECORD_FURTHEST(parser);
              }
            }
          } else {
            parser->success = false;
            if (has_terminal) {
              parser->pos = last_terminal_pos;
              parser->success = true;
            } else {
              PGEN_RECORD_FURTHEST(parser);
            }
          }
          break;
        default:
          parser->success = false;
          if (has_terminal) {
            parser->pos = last_terminal_pos;
            parser->success = true;
          } else {
            PGEN_RECORD_FURTHEST(parser);
          }
        }
      } else {
        parser->success = false;
        if (has_terminal) {
          parser->pos = last_terminal_pos;
          parser->success = true;
        } else {
          PGEN_RECORD_FURTHEST(parser);
        }
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, trie_start);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test3", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test3", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_test4(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test4", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Capture
      size_t start_pos = parser->pos;
      { // Trie match for: "xx", "xy", "xz"
        REMEMBER_POSITION(parser, trie_start);
        size_t last_terminal_pos = 0;
        int has_terminal = 0;

        if (parser->pos < parser->input_len) {
          switch (parser->input[parser->pos]) {
          case 120: // "x"
            parser->pos++;

            if (parser->pos < parser->input_len) {
              switch (parser->input[parser->pos]) {
              case 120: // "x"
                parser->pos++;
                break;
              case 121: // "y"
                parser->pos++;
                break;
              case 122: // "z"
                parser->pos++;
                break;
              default:
                parser->success = false;
                if (has_terminal) {
                  parser->pos = last_terminal_pos;
                  parser->success = true;
                } else {
                  PGEN_RECORD_FURTHEST(parser);
                }
              }
            } else {
              parser->success = false;
              if (has_terminal) {
                parser->pos = last_terminal_pos;
                parser->success = true;
              } else {
                PGEN_RECORD_FURTHEST(parser);
              }
            }
            break;
          default:
            parser->success = false;
            if (has_terminal) {
              parser->pos = last_terminal_pos;
              parser->success = true;
            } else {
              PGEN_RECORD_FURTHEST(parser);
            }
          }
        } else {
          parser->success = false;
          if (has_terminal) {
            parser->pos = last_terminal_pos;
            parser->success = true;
          } else {
            PGEN_RECORD_FURTHEST(parser);
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, trie_start);
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
        { // Match character set "abc"
          if (parser->pos < parser->input_len) {
            switch (parser->input[parser->pos]) {
            case 97: /* "a" */
            case 98: /* "b" */
            case 99: /* "c" */
              parser->pos++;
              break;
            default:
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected one of "
                                             "\"abc\""
                                             " at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected one of "
                                           "\"abc\""
                                           " at position %zu but reached end of input",
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
        RESTORE_POSITION(parser, pos);
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
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_test5(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "test5", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Capture
      size_t start_pos = parser->pos;
      { // Trie match for: "abcd", "ab", "ax"
        REMEMBER_POSITION(parser, trie_start);
        size_t last_terminal_pos = 0;
        int has_terminal = 0;

        if (parser->pos < parser->input_len) {
          switch (parser->input[parser->pos]) {
          case 97: // "a"
            parser->pos++;

            if (parser->pos < parser->input_len) {
              switch (parser->input[parser->pos]) {
              case 98: // "b"
                parser->pos++;
                if (!has_terminal || parser->pos > last_terminal_pos) {
                  last_terminal_pos = parser->pos;
                  has_terminal = 1;
                }
                if (parser->pos < parser->input_len) {
                  switch (parser->input[parser->pos]) {
                  case 99: // "c"
                    parser->pos++;

                    if (parser->pos < parser->input_len) {
                      switch (parser->input[parser->pos]) {
                      case 100: // "d"
                        parser->pos++;
                        break;
                      default:
                        parser->success = false;
                        if (has_terminal) {
                          parser->pos = last_terminal_pos;
                          parser->success = true;
                        } else {
                          PGEN_RECORD_FURTHEST(parser);
                        }
                      }
                    } else {
                      parser->success = false;
                      if (has_terminal) {
                        parser->pos = last_terminal_pos;
                        parser->success = true;
                      } else {
                        PGEN_RECORD_FURTHEST(parser);
                      }
                    }
                    break;
                  default:
                    parser->success = false;
                    if (has_terminal) {
                      parser->pos = last_terminal_pos;
                      parser->success = true;
                    } else {
                      PGEN_RECORD_FURTHEST(parser);
                    }
                  }
                } else {
                  parser->success = false;
                  if (has_terminal) {
                    parser->pos = last_terminal_pos;
                    parser->success = true;
                  } else {
                    PGEN_RECORD_FURTHEST(parser);
                  }
                }
                if (!parser->success) {
                  // Partial match is valid: "ab"
                  parser->success = true;
                }
                break;
              case 120: // "x"
                parser->pos++;
                break;
              default:
                parser->success = false;
                if (has_terminal) {
                  parser->pos = last_terminal_pos;
                  parser->success = true;
                } else {
                  PGEN_RECORD_FURTHEST(parser);
                }
              }
            } else {
              parser->success = false;
              if (has_terminal) {
                parser->pos = last_terminal_pos;
                parser->success = true;
              } else {
                PGEN_RECORD_FURTHEST(parser);
              }
            }
            break;
          default:
            parser->success = false;
            if (has_terminal) {
              parser->pos = last_terminal_pos;
              parser->success = true;
            } else {
              PGEN_RECORD_FURTHEST(parser);
            }
          }
        } else {
          parser->success = false;
          if (has_terminal) {
            parser->pos = last_terminal_pos;
            parser->success = true;
          } else {
            PGEN_RECORD_FURTHEST(parser);
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, trie_start);
        }
      }

      if (parser->success) {
        size_t capture_length = parser->pos - start_pos;
        pgen_checkstack(parser, 1);
        lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
      }
    }
    if (parser->success) {
      { // Match single character "X"
        if (parser->pos < parser->input_len &&
            parser->input[parser->pos] == 88) {
          parser->pos++;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected character `"
                                         "X"
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "test5", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "test5", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

// Initialize parser
static Parser *trie_optimization_init(const char *input, lua_State *L) {
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
static void trie_optimization_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_trie_optimization_parse(lua_State *L) {
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
  Parser *parser = trie_optimization_init(input, L);
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
      trie_optimization_free(parser);
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
      trie_optimization_free(parser);
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
    trie_optimization_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  trie_optimization_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg trie_optimization_module[] = {
    {"parse", l_trie_optimization_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}                          // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_trie_optimization(lua_State *L) {

  luaL_newlib(L, trie_optimization_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register. Register into a fresh table rather than a
// named global: a name would be shared through package.loaded, so loading
// two parsers compiled with the same parser_name in one process would
// silently overwrite the first module's parse function.
int luaopen_trie_optimization(lua_State *L) {

  lua_newtable(L);
  luaL_register(L, NULL, trie_optimization_module);
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o trie_optimization.so -fPIC trie_optimization.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local trie_optimization = require "trie_optimization"
local result = trie_optimization.parse("your input string")
*/

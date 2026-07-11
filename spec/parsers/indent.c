#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// indent - generated parser

// Maximum rule-call recursion depth before the parse is aborted with a Lua
// error (prevents C stack overflow on deeply nested input). Override with
// the max_depth compile option or -DPGEN_MAX_DEPTH=n
#ifndef PGEN_MAX_DEPTH
#define PGEN_MAX_DEPTH 5000
#endif

#include <limits.h>

// Indenter (match-time integer stack) infrastructure
#define PGEN_HAS_IND 1
#define PGEN_IND_STACK_COUNT 3
// Sentinel pushed by `prevent`: no measured width compares greater than it,
// so any nested `advance` fails
#define PGEN_IND_PREVENT_SENTINEL INT_MAX

typedef struct {
  int *items;
  int size;
  int cap;
} PgenIndStack;

// Undo log entry for transactional stack operations. Rewinding the trail on
// backtrack reverses every push/pop performed since the choice point.
typedef struct {
  unsigned char stack_id;
  unsigned char op; // 0 = push, 1 = pop
  int value;        // for pop entries: the value that was popped
} PgenTrailEntry;

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
  PgenIndStack ind_stacks[PGEN_IND_STACK_COUNT];
  PgenTrailEntry *trail;
  size_t trail_len;
  size_t trail_cap;
} Parser;

typedef struct {
  size_t pos;
  int stack_size;
  size_t trail_index;
} ParserPosition;

typedef struct {
  size_t pos;
} ParserInputPosition;

#define REMEMBER_POSITION(parser, pp)        \
  ParserPosition pp;                         \
  (pp).pos = (parser)->pos;                  \
  (pp).stack_size = lua_gettop((parser)->L); \
  (pp).trail_index = (parser)->trail_len;

// Restore parser position
#define RESTORE_POSITION(parser, pp)        \
  (parser)->pos = (pp).pos;                 \
  lua_settop((parser)->L, (pp).stack_size); \
  pgen_ind_trail_rewind(parser, (pp).trail_index);

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

// Rewind the indenter trail to a previous length, undoing pushes and pops
static void pgen_ind_trail_rewind(Parser *parser, size_t index) {
  while (parser->trail_len > index) {
    parser->trail_len--;
    PgenTrailEntry *e = &parser->trail[parser->trail_len];
    PgenIndStack *s = &parser->ind_stacks[e->stack_id];
    if (e->op == 0) {
      // undo push
      s->size--;
    } else {
      // undo pop: the slot is still allocated, restore the value
      s->items[s->size] = e->value;
      s->size++;
    }
  }
}

static void pgen_ind_trail_record(Parser *parser, int stack_id, int op, int value) {
  if (parser->trail_len >= parser->trail_cap) {
    size_t new_cap = parser->trail_cap == 0 ? 64 : parser->trail_cap * 2;
    PgenTrailEntry *trail = (PgenTrailEntry *)realloc(parser->trail, new_cap * sizeof(PgenTrailEntry));
    if (!trail) {
      luaL_error(parser->L, "pgen: out of memory growing indenter trail");
    }
    parser->trail = trail;
    parser->trail_cap = new_cap;
  }
  parser->trail[parser->trail_len].stack_id = (unsigned char)stack_id;
  parser->trail[parser->trail_len].op = (unsigned char)op;
  parser->trail[parser->trail_len].value = value;
  parser->trail_len++;
}

static void pgen_ind_push(Parser *parser, int stack_id, int value) {
  PgenIndStack *s = &parser->ind_stacks[stack_id];
  if (s->size >= s->cap) {
    int new_cap = s->cap * 2;
    int *items = (int *)realloc(s->items, new_cap * sizeof(int));
    if (!items) {
      luaL_error(parser->L, "pgen: out of memory growing indenter stack");
    }
    s->items = items;
    s->cap = new_cap;
  }
  s->items[s->size++] = value;
  pgen_ind_trail_record(parser, stack_id, 0, value);
}

// Pop the stack; returns false if the stack is empty
static bool pgen_ind_pop(Parser *parser, int stack_id) {
  PgenIndStack *s = &parser->ind_stacks[stack_id];
  if (s->size == 0) {
    return false;
  }
  s->size--;
  pgen_ind_trail_record(parser, stack_id, 1, s->items[s->size]);
  return true;
}

// Measure the indentation width of the run of space/tab characters at the
// current position (space = 1, tab = tab_width). Sets *end_pos to the first
// position past the run.
static int pgen_ind_measure(Parser *parser, size_t *end_pos, int tab_width) {
  size_t p = parser->pos;
  int width = 0;
  while (p < parser->input_len) {
    char c = parser->input[p];
    if (c == ' ') {
      width += 1;
    } else if (c == '\t') {
      width += tab_width;
    } else {
      break;
    }
    p++;
  }
  *end_pos = p;
  return width;
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
// Interned constant strings (pushed once at module load)
static int __const_refs[14];

static void __const_init(lua_State *L) {
  lua_pushlstring(L, "advanced", 8);
  __const_refs[0] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "allowed", 7);
  __const_refs[1] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "blocked", 7);
  __const_refs[2] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "clean", 5);
  __const_refs[3] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "cmt-clean", 9);
  __const_refs[4] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "disabled", 8);
  __const_refs[5] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "double", 6);
  __const_refs[6] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "never", 5);
  __const_refs[7] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "ok", 2);
  __const_refs[8] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "pure", 4);
  __const_refs[9] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "single", 6);
  __const_refs[10] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "tab2", 4);
  __const_refs[11] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "tab4", 4);
  __const_refs[12] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "top-check", 9);
  __const_refs[13] = luaL_ref(L, LUA_REGISTRYINDEX);
}
// Match-time capture (Cmt) infrastructure

static const char __cmt_code_0[] = "return false";

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
static bool parse_Block(Parser *parser);
static bool parse_Line(Parser *parser);
static bool parse_Stmt(Parser *parser);
static bool parse_advance_control(Parser *parser);
static bool parse_allowed(Parser *parser);
static bool parse_backtrack_test(Parser *parser);
static bool parse_block_test(Parser *parser);
static bool parse_bt_fail(Parser *parser);
static bool parse_bt_fallback(Parser *parser);
static bool parse_cmt_test(Parser *parser);
static bool parse_flags_test(Parser *parser);
static bool parse_lookahead_test(Parser *parser);
static bool parse_name(Parser *parser);
static bool parse_nl(Parser *parser);
static bool parse_pop_test(Parser *parser);
static bool parse_pr_advanced(Parser *parser);
static bool parse_pr_blocked(Parser *parser);
static bool parse_prevent_test(Parser *parser);
static bool parse_push_test(Parser *parser);
static bool parse_tab2_test(Parser *parser);
static bool parse_tab4_test(Parser *parser);

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

  {                     // Choice
    {                   // Choice
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

                        { // Match literal "10:"
                          if (parser->pos + 3 <= parser->input_len &&
                              memcmp(parser->input + parser->pos, "10:", 3) == 0) {
                            parser->pos += 3;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message, "Expected `"
                                                           "10:"
                                                           "` at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                            PGEN_RECORD_FURTHEST(parser);
                          }
                        }
                        if (parser->success) {
                          parse_tab4_test(parser);
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

                          { // Match literal "11:"
                            if (parser->pos + 3 <= parser->input_len &&
                                memcmp(parser->input + parser->pos, "11:", 3) == 0) {
                              parser->pos += 3;
                            } else {
#ifdef PGEN_ERRORS
                              sprintf(parser->error_message, "Expected `"
                                                             "11:"
                                                             "` at position %zu",
                                      parser->pos);
#endif
                              parser->success = false;
                              PGEN_RECORD_FURTHEST(parser);
                            }
                          }
                          if (parser->success) {
                            parse_tab2_test(parser);
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
                          parse_block_test(parser);
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
                        parse_backtrack_test(parser);
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
                      parse_prevent_test(parser);
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
                    parse_advance_control(parser);
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
                  parse_push_test(parser);
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
                parse_flags_test(parser);
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
              parse_pop_test(parser);
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
            parse_lookahead_test(parser);
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
          parse_cmt_test(parser);
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

static bool parse_Block(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Block", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      parse_Line(parser);
      if (parser->success) {
        { // Zero or more repetitions
          while (true) {
            { // Sequence with 2 patterns
              REMEMBER_POSITION(parser, pos);

              parse_nl(parser);
              if (parser->success) {
                parse_Line(parser);
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
          int sentinel_idx = cg_sentinel_index(lua_touserdata(parser->L, i));
          if (sentinel_idx >= 0) {
            // Named capture: sentinel at i, value at i+1; name interned at load
            lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cg_name_refs[sentinel_idx]);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Block", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Block", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_Line(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Line", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Indenter check (stack 0): consume whitespace, width must equal top
      size_t ind_end;
      int ind_width = pgen_ind_measure(parser, &ind_end, 4);
      PgenIndStack *ind_s = &parser->ind_stacks[0];
      if (ind_s->size > 0 && ind_s->items[ind_s->size - 1] == ind_width) {
        parser->pos = ind_end;
      } else {
        parser->success = false;
        PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Indent width %d does not match current level at position %zu", ind_width, parser->pos);
#endif
      }
    }
    if (parser->success) {
      parse_Stmt(parser);
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Line", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Line", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_Stmt(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Stmt", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    parse_name(parser);
    if (parser->success) {
      { // At most 1 repetitions
        size_t rep_count = 0;

        while (rep_count < 1) {
          size_t before_pos = parser->pos;

          {
            { // Sequence with 5 patterns
              REMEMBER_POSITION(parser, pos);

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
                parse_nl(parser);
                if (parser->success) {
                  { // Indenter advance (stack 0): push width if deeper than top, consume nothing
                    size_t ind_end;
                    int ind_width = pgen_ind_measure(parser, &ind_end, 4);
                    (void)ind_end;
                    PgenIndStack *ind_s = &parser->ind_stacks[0];
                    if (ind_s->size > 0 && ind_width > ind_s->items[ind_s->size - 1]) {
                      pgen_ind_push(parser, 0, ind_width);
                    } else {
                      parser->success = false;
                      PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message, "Indent width %d does not advance current level at position %zu", ind_width, parser->pos);
#endif
                    }
                  }
                  if (parser->success) {
                    parse_Block(parser);
                    if (parser->success) {
                      { // Indenter pop (stack 0)
                        if (!pgen_ind_pop(parser, 0)) {
                          parser->success = false;
                          PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message, "Indenter stack 0 is empty at position %zu", parser->pos);
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Stmt", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Stmt", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_advance_control(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "advance_control", start);
#endif

  { // Choice
    parse_pr_advanced(parser);

    // Only try alternative if ordinary failure (not labeled failure from T())
    if (!parser->success && !parser->throw_label) {
      parser->success = true;
      parse_pr_blocked(parser);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "advance_control", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "advance_control", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_allowed(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "allowed", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Indenter ctop (stack 1): top ne 0
      PgenIndStack *ind_s = &parser->ind_stacks[1];
      if (!(ind_s->size > 0 && ind_s->items[ind_s->size - 1] != 0)) {
        parser->success = false;
        PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Indenter stack 1 top failed ne 0 check at position %zu", parser->pos);
#endif
      }
    }
    if (parser->success) {
      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_checkstack(parser, 1);
        lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[1]); // "allowed"
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "allowed", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "allowed", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_backtrack_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "backtrack_test", start);
#endif

  { // Choice
    parse_bt_fail(parser);

    // Only try alternative if ordinary failure (not labeled failure from T())
    if (!parser->success && !parser->throw_label) {
      parser->success = true;
      parse_bt_fallback(parser);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "backtrack_test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "backtrack_test", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_block_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "block_test", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    parse_Block(parser);
    if (parser->success) {
      { // Negate (only match if pattern fails)
        REMEMBER_INPUT_POSITION(parser, pos);

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
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "block_test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "block_test", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_bt_fail(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "bt_fail", start);
#endif

  { // Sequence with 4 patterns
    REMEMBER_POSITION(parser, pos);

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
      parse_nl(parser);
      if (parser->success) {
        { // Indenter advance (stack 0): push width if deeper than top, consume nothing
          size_t ind_end;
          int ind_width = pgen_ind_measure(parser, &ind_end, 4);
          (void)ind_end;
          PgenIndStack *ind_s = &parser->ind_stacks[0];
          if (ind_s->size > 0 && ind_width > ind_s->items[ind_s->size - 1]) {
            pgen_ind_push(parser, 0, ind_width);
          } else {
            parser->success = false;
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Indent width %d does not advance current level at position %zu", ind_width, parser->pos);
#endif
          }
        }
        if (parser->success) {
          { // Match literal "zzz"
            if (parser->pos + 3 <= parser->input_len &&
                memcmp(parser->input + parser->pos, "zzz", 3) == 0) {
              parser->pos += 3;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected `"
                                             "zzz"
                                             "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
              PGEN_RECORD_FURTHEST(parser);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "bt_fail", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "bt_fail", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_bt_fallback(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "bt_fallback", start);
#endif

  { // Sequence with 6 patterns
    REMEMBER_POSITION(parser, pos);

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
      parse_nl(parser);
      if (parser->success) {
        { // Indenter ctop (stack 0): top eq 0
          PgenIndStack *ind_s = &parser->ind_stacks[0];
          if (!(ind_s->size > 0 && ind_s->items[ind_s->size - 1] == 0)) {
            parser->success = false;
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Indenter stack 0 top failed eq 0 check at position %zu", parser->pos);
#endif
          }
        }
        if (parser->success) {
          { // Zero or more repetitions
            while (true) {
              { // Match character set " "
                if (parser->pos < parser->input_len) {
                  switch (parser->input[parser->pos]) {
                  case 32: /* " " */
                    parser->pos++;
                    break;
                  default:
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Expected one of "
                                                   "\" \""
                                                   " at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected one of "
                                                 "\" \""
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
              { // Constant Capture
                // A constant capture matches the empty string and produces all given values
                pgen_checkstack(parser, 1);
                lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[3]); // "clean"
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "bt_fallback", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "bt_fallback", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_cmt_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "cmt_test", start);
#endif

  {   // Choice
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Match-time capture (Cmt id=0)
        int cmt_stack_base = lua_gettop(parser->L);
        size_t cmt_start_pos = parser->pos;
#ifdef PGEN_HAS_IND
        size_t cmt_trail_index = parser->trail_len;
#endif

        { // Indenter cpush (stack 0): push constant 7
          pgen_ind_push(parser, 0, 7);
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
        { // Constant Capture
          // A constant capture matches the empty string and produces all given values
          pgen_checkstack(parser, 1);
          lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[7]); // "never"
        }
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

        { // Indenter ctop (stack 0): top eq 0
          PgenIndStack *ind_s = &parser->ind_stacks[0];
          if (!(ind_s->size > 0 && ind_s->items[ind_s->size - 1] == 0)) {
            parser->success = false;
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Indenter stack 0 top failed eq 0 check at position %zu", parser->pos);
#endif
          }
        }
        if (parser->success) {
          { // Constant Capture
            // A constant capture matches the empty string and produces all given values
            pgen_checkstack(parser, 1);
            lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[4]); // "cmt-clean"
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "cmt_test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "cmt_test", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_flags_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "flags_test", start);
#endif

  { // Sequence with 5 patterns
    REMEMBER_POSITION(parser, pos);

    parse_allowed(parser);
    if (parser->success) {
      { // Indenter cpush (stack 1): push constant 0
        pgen_ind_push(parser, 1, 0);
      }
      if (parser->success) {
        { // Choice
          parse_allowed(parser);

          // Only try alternative if ordinary failure (not labeled failure from T())
          if (!parser->success && !parser->throw_label) {
            parser->success = true;
            { // Constant Capture
              // A constant capture matches the empty string and produces all given values
              pgen_checkstack(parser, 1);
              lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[5]); // "disabled"
            }
          }
        }
        if (parser->success) {
          { // Indenter pop (stack 1)
            if (!pgen_ind_pop(parser, 1)) {
              parser->success = false;
              PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Indenter stack 1 is empty at position %zu", parser->pos);
#endif
            }
          }
          if (parser->success) {
            parse_allowed(parser);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "flags_test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "flags_test", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_lookahead_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "lookahead_test", start);
#endif

  { // Sequence with 3 patterns
    REMEMBER_POSITION(parser, pos);

    { // Lookahead (match without consuming input)
      REMEMBER_POSITION(parser, pos);

      { // Indenter cpush (stack 0): push constant 9
        pgen_ind_push(parser, 0, 9);
      }

      if (parser->success) {
        // Pattern matched, but we don't consume any input
        RESTORE_POSITION(parser, pos);
      }
    }
    if (parser->success) {
      { // Indenter ctop (stack 0): top eq 0
        PgenIndStack *ind_s = &parser->ind_stacks[0];
        if (!(ind_s->size > 0 && ind_s->items[ind_s->size - 1] == 0)) {
          parser->success = false;
          PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Indenter stack 0 top failed eq 0 check at position %zu", parser->pos);
#endif
        }
      }
      if (parser->success) {
        { // Constant Capture
          // A constant capture matches the empty string and produces all given values
          pgen_checkstack(parser, 1);
          lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[9]); // "pure"
        }
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "lookahead_test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "lookahead_test", parser->pos);
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
      REMEMBER_INPUT_POSITION(parser, pos);
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
        RESTORE_INPUT_POSITION(parser, pos);
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

static bool parse_nl(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "nl", start);
#endif

  { // Match single character "\n"
    if (parser->pos < parser->input_len &&
        parser->input[parser->pos] == 10) {
      parser->pos++;
    } else {
#ifdef PGEN_ERRORS
      sprintf(parser->error_message, "Expected character `"
                                     "\\n"
                                     "` at position %zu",
              parser->pos);
#endif
      parser->success = false;
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "nl", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "nl", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_pop_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "pop_test", start);
#endif

  {     // Choice
    {   // Choice
      { // Sequence with 3 patterns
        REMEMBER_POSITION(parser, pos);

        { // Indenter pop (stack 0)
          if (!pgen_ind_pop(parser, 0)) {
            parser->success = false;
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Indenter stack 0 is empty at position %zu", parser->pos);
#endif
          }
        }
        if (parser->success) {
          { // Indenter pop (stack 0)
            if (!pgen_ind_pop(parser, 0)) {
              parser->success = false;
              PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Indenter stack 0 is empty at position %zu", parser->pos);
#endif
            }
          }
          if (parser->success) {
            { // Constant Capture
              // A constant capture matches the empty string and produces all given values
              pgen_checkstack(parser, 1);
              lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[6]); // "double"
            }
          }
          if (!parser->success) {
            RESTORE_POSITION(parser, pos);
          }
        }
      }

      // Only try alternative if ordinary failure (not labeled failure from T())
      if (!parser->success && !parser->throw_label) {
        parser->success = true;
        { // Sequence with 3 patterns
          REMEMBER_POSITION(parser, pos);

          { // Indenter pop (stack 0)
            if (!pgen_ind_pop(parser, 0)) {
              parser->success = false;
              PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Indenter stack 0 is empty at position %zu", parser->pos);
#endif
            }
          }
          if (parser->success) {
            { // Indenter ctop (stack 0): top eq 99
              PgenIndStack *ind_s = &parser->ind_stacks[0];
              if (!(ind_s->size > 0 && ind_s->items[ind_s->size - 1] == 99)) {
                parser->success = false;
                PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Indenter stack 0 top failed eq 99 check at position %zu", parser->pos);
#endif
              }
            }
            if (parser->success) {
              { // Constant Capture
                // A constant capture matches the empty string and produces all given values
                pgen_checkstack(parser, 1);
                lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[13]); // "top-check"
              }
            }
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

        { // Indenter pop (stack 0)
          if (!pgen_ind_pop(parser, 0)) {
            parser->success = false;
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Indenter stack 0 is empty at position %zu", parser->pos);
#endif
          }
        }
        if (parser->success) {
          { // Constant Capture
            // A constant capture matches the empty string and produces all given values
            pgen_checkstack(parser, 1);
            lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[10]); // "single"
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "pop_test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "pop_test", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_pr_advanced(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "pr_advanced", start);
#endif

  { // Sequence with 7 patterns
    REMEMBER_POSITION(parser, pos);

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
      parse_nl(parser);
      if (parser->success) {
        { // Indenter advance (stack 0): push width if deeper than top, consume nothing
          size_t ind_end;
          int ind_width = pgen_ind_measure(parser, &ind_end, 4);
          (void)ind_end;
          PgenIndStack *ind_s = &parser->ind_stacks[0];
          if (ind_s->size > 0 && ind_width > ind_s->items[ind_s->size - 1]) {
            pgen_ind_push(parser, 0, ind_width);
          } else {
            parser->success = false;
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Indent width %d does not advance current level at position %zu", ind_width, parser->pos);
#endif
          }
        }
        if (parser->success) {
          { // Zero or more repetitions
            while (true) {
              { // Match character set " "
                if (parser->pos < parser->input_len) {
                  switch (parser->input[parser->pos]) {
                  case 32: /* " " */
                    parser->pos++;
                    break;
                  default:
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Expected one of "
                                                   "\" \""
                                                   " at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected one of "
                                                 "\" \""
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
              { // Indenter pop (stack 0)
                if (!pgen_ind_pop(parser, 0)) {
                  parser->success = false;
                  PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Indenter stack 0 is empty at position %zu", parser->pos);
#endif
                }
              }
              if (parser->success) {
                { // Constant Capture
                  // A constant capture matches the empty string and produces all given values
                  pgen_checkstack(parser, 1);
                  lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[0]); // "advanced"
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "pr_advanced", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "pr_advanced", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_pr_blocked(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "pr_blocked", start);
#endif

  { // Sequence with 5 patterns
    REMEMBER_POSITION(parser, pos);

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
      parse_nl(parser);
      if (parser->success) {
        { // Zero or more repetitions
          while (true) {
            { // Match character set " "
              if (parser->pos < parser->input_len) {
                switch (parser->input[parser->pos]) {
                case 32: /* " " */
                  parser->pos++;
                  break;
                default:
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Expected one of "
                                                 "\" \""
                                                 " at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message, "Expected one of "
                                               "\" \""
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
            { // Constant Capture
              // A constant capture matches the empty string and produces all given values
              pgen_checkstack(parser, 1);
              lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[2]); // "blocked"
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "pr_blocked", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "pr_blocked", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_prevent_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "prevent_test", start);
#endif

  { // Sequence with 3 patterns
    REMEMBER_POSITION(parser, pos);

    { // Indenter prevent (stack 0): push sentinel so nested advance fails
      pgen_ind_push(parser, 0, PGEN_IND_PREVENT_SENTINEL);
    }
    if (parser->success) {
      { // Choice
        parse_pr_advanced(parser);

        // Only try alternative if ordinary failure (not labeled failure from T())
        if (!parser->success && !parser->throw_label) {
          parser->success = true;
          parse_pr_blocked(parser);
        }
      }
      if (parser->success) {
        { // Indenter pop (stack 0)
          if (!pgen_ind_pop(parser, 0)) {
            parser->success = false;
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Indenter stack 0 is empty at position %zu", parser->pos);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "prevent_test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "prevent_test", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_push_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "push_test", start);
#endif

  { // Sequence with 10 patterns
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
      parse_nl(parser);
      if (parser->success) {
        { // Indenter push (stack 0): consume whitespace, push measured width
          size_t ind_end;
          int ind_width = pgen_ind_measure(parser, &ind_end, 4);
          pgen_ind_push(parser, 0, ind_width);
          parser->pos = ind_end;
        }
        if (parser->success) {
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
            parse_nl(parser);
            if (parser->success) {
              { // Indenter check (stack 0): consume whitespace, width must equal top
                size_t ind_end;
                int ind_width = pgen_ind_measure(parser, &ind_end, 4);
                PgenIndStack *ind_s = &parser->ind_stacks[0];
                if (ind_s->size > 0 && ind_s->items[ind_s->size - 1] == ind_width) {
                  parser->pos = ind_end;
                } else {
                  parser->success = false;
                  PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message, "Indent width %d does not match current level at position %zu", ind_width, parser->pos);
#endif
                }
              }
              if (parser->success) {
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
                  { // Indenter pop (stack 0)
                    if (!pgen_ind_pop(parser, 0)) {
                      parser->success = false;
                      PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message, "Indenter stack 0 is empty at position %zu", parser->pos);
#endif
                    }
                  }
                  if (parser->success) {
                    { // Negate (only match if pattern fails)
                      REMEMBER_INPUT_POSITION(parser, pos);

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
                      { // Constant Capture
                        // A constant capture matches the empty string and produces all given values
                        pgen_checkstack(parser, 1);
                        lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[8]); // "ok"
                      }
                    }
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "push_test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "push_test", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_tab2_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "tab2_test", start);
#endif

  { // Sequence with 8 patterns
    REMEMBER_POSITION(parser, pos);

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
      parse_nl(parser);
      if (parser->success) {
        { // Indenter advance (stack 2): push width if deeper than top, consume nothing
          size_t ind_end;
          int ind_width = pgen_ind_measure(parser, &ind_end, 2);
          (void)ind_end;
          PgenIndStack *ind_s = &parser->ind_stacks[2];
          if (ind_s->size > 0 && ind_width > ind_s->items[ind_s->size - 1]) {
            pgen_ind_push(parser, 2, ind_width);
          } else {
            parser->success = false;
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Indent width %d does not advance current level at position %zu", ind_width, parser->pos);
#endif
          }
        }
        if (parser->success) {
          { // Indenter ctop (stack 2): top eq 2
            PgenIndStack *ind_s = &parser->ind_stacks[2];
            if (!(ind_s->size > 0 && ind_s->items[ind_s->size - 1] == 2)) {
              parser->success = false;
              PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Indenter stack 2 top failed eq 2 check at position %zu", parser->pos);
#endif
            }
          }
          if (parser->success) {
            { // Zero or more repetitions
              while (true) {
                { // Match character set " \t"
                  if (parser->pos < parser->input_len) {
                    switch (parser->input[parser->pos]) {
                    case 32: /* " " */
                    case 9:  /* "\t" */
                      parser->pos++;
                      break;
                    default:
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message, "Expected one of "
                                                     "\" \\t\""
                                                     " at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Expected one of "
                                                   "\" \\t\""
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
                { // Indenter pop (stack 2)
                  if (!pgen_ind_pop(parser, 2)) {
                    parser->success = false;
                    PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Indenter stack 2 is empty at position %zu", parser->pos);
#endif
                  }
                }
                if (parser->success) {
                  { // Constant Capture
                    // A constant capture matches the empty string and produces all given values
                    pgen_checkstack(parser, 1);
                    lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[11]); // "tab2"
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "tab2_test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "tab2_test", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_tab4_test(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "tab4_test", start);
#endif

  { // Sequence with 8 patterns
    REMEMBER_POSITION(parser, pos);

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
      parse_nl(parser);
      if (parser->success) {
        { // Indenter advance (stack 0): push width if deeper than top, consume nothing
          size_t ind_end;
          int ind_width = pgen_ind_measure(parser, &ind_end, 4);
          (void)ind_end;
          PgenIndStack *ind_s = &parser->ind_stacks[0];
          if (ind_s->size > 0 && ind_width > ind_s->items[ind_s->size - 1]) {
            pgen_ind_push(parser, 0, ind_width);
          } else {
            parser->success = false;
            PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Indent width %d does not advance current level at position %zu", ind_width, parser->pos);
#endif
          }
        }
        if (parser->success) {
          { // Indenter ctop (stack 0): top eq 4
            PgenIndStack *ind_s = &parser->ind_stacks[0];
            if (!(ind_s->size > 0 && ind_s->items[ind_s->size - 1] == 4)) {
              parser->success = false;
              PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Indenter stack 0 top failed eq 4 check at position %zu", parser->pos);
#endif
            }
          }
          if (parser->success) {
            { // Zero or more repetitions
              while (true) {
                { // Match character set " \t"
                  if (parser->pos < parser->input_len) {
                    switch (parser->input[parser->pos]) {
                    case 32: /* " " */
                    case 9:  /* "\t" */
                      parser->pos++;
                      break;
                    default:
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message, "Expected one of "
                                                     "\" \\t\""
                                                     " at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Expected one of "
                                                   "\" \\t\""
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
                { // Indenter pop (stack 0)
                  if (!pgen_ind_pop(parser, 0)) {
                    parser->success = false;
                    PGEN_RECORD_FURTHEST(parser);
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message, "Indenter stack 0 is empty at position %zu", parser->pos);
#endif
                  }
                }
                if (parser->success) {
                  { // Constant Capture
                    // A constant capture matches the empty string and produces all given values
                    pgen_checkstack(parser, 1);
                    lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __const_refs[12]); // "tab4"
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "tab4_test", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "tab4_test", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

// Initialize parser
static Parser *indent_init(const char *input, lua_State *L) {
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

  // Initialize indenter stacks (each starts holding its initial value)
  static const int pgen_ind_initials[PGEN_IND_STACK_COUNT] = {0, 1, 0};
  parser->trail = NULL;
  parser->trail_len = 0;
  parser->trail_cap = 0;
  for (int i = 0; i < PGEN_IND_STACK_COUNT; i++) {
    parser->ind_stacks[i].items = (int *)malloc(8 * sizeof(int));
    if (!parser->ind_stacks[i].items) {
      for (int j = 0; j < i; j++) {
        free(parser->ind_stacks[j].items);
      }
      free(parser);
      return NULL;
    }
    parser->ind_stacks[i].cap = 8;
    parser->ind_stacks[i].size = 1;
    parser->ind_stacks[i].items[0] = pgen_ind_initials[i];
  }

  return parser;
}

// Free parser
static void indent_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    for (int i = 0; i < PGEN_IND_STACK_COUNT; i++) {
      free(parser->ind_stacks[i].items);
    }
    free(parser->trail);
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_indent_parse(lua_State *L) {
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
  Parser *parser = indent_init(input, L);
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
      indent_free(parser);
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
      indent_free(parser);
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
    indent_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  indent_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg indent_module[] = {
    {"parse", l_indent_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}               // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_indent(lua_State *L) {
  __const_init(L);
  __cmt_init(L);
  luaL_newlib(L, indent_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register. Register into a fresh table rather than a
// named global: a name would be shared through package.loaded, so loading
// two parsers compiled with the same parser_name in one process would
// silently overwrite the first module's parse function.
int luaopen_indent(lua_State *L) {
  __const_init(L);
  __cmt_init(L);
  lua_newtable(L);
  luaL_register(L, NULL, indent_module);
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o indent.so -fPIC indent.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local indent = require "indent"
local result = indent.parse("your input string")
*/

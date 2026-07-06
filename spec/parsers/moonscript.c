#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// moonscript - generated parser

// Maximum rule-call recursion depth before the parse is aborted with a Lua
// error (prevents C stack overflow on deeply nested input). Override with
// the max_depth compile option or -DPGEN_MAX_DEPTH=n
#ifndef PGEN_MAX_DEPTH
#define PGEN_MAX_DEPTH 5000
#endif

#include <limits.h>

// Indenter (match-time integer stack) infrastructure
#define PGEN_HAS_IND 1
#define PGEN_IND_STACK_COUNT 1
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

// No Cg sentinels defined - stub function
static bool is_cg_sentinel(void *ptr) {
  (void)ptr; // unused
  return false;
}

// Forward declarations
static bool parse_File(Parser *parser);
static bool parse_Assign(Parser *parser);
static bool parse_BlankLine(Parser *parser);
static bool parse_Block(Parser *parser);
static bool parse_IfStatement(Parser *parser);
static bool parse_InBlock(Parser *parser);
static bool parse_Line(Parser *parser);
static bool parse_Name(Parser *parser);
static bool parse_Number(Parser *parser);
static bool parse_Statement(Parser *parser);
static bool parse_String(Parser *parser);
static bool parse_Value(Parser *parser);
static bool parse_ident(Parser *parser);
static bool parse_nl(Parser *parser);
static bool parse_ws(Parser *parser);

// Rule functions
static bool parse_File(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "File", start);
#endif

  { // Sequence with 3 patterns
    REMEMBER_POSITION(parser, pos);

    parse_Block(parser);
    if (parser->success) {
      parse_ws(parser);
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
            }
          }

          if (parser->success) {
            // Pattern matched, so negate fails
            RESTORE_POSITION(parser, pos);
            parser->success = false;
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "File", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "File", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_Assign(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Assign", start);
#endif

  { // Capture Table (array-only)
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 6 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_checkstack(parser, 1);
        lua_pushlstring(parser->L, "assign", 6);
      }
      if (parser->success) {
        parse_Name(parser);
        if (parser->success) {
          parse_ws(parser);
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
              parse_ws(parser);
              if (parser->success) {
                parse_Value(parser);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Assign", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Assign", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_BlankLine(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "BlankLine", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

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
      parse_nl(parser);
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "BlankLine", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "BlankLine", parser->pos);
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
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_checkstack(parser, 1);
        lua_pushlstring(parser->L, "block", 5);
      }
      if (parser->success) {
        { // At least 1 repetitions
          REMEMBER_POSITION(parser, pos);
          size_t rep_count = 0;

          while (true) {
            { // Choice
              parse_BlankLine(parser);

              // Only try alternative if ordinary failure (not labeled failure from T())
              if (!parser->success && !parser->throw_label) {
                parser->success = true;
                parse_Line(parser);
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
          { // Zero or more repetitions
            while (true) {
              { // Choice
                parse_BlankLine(parser);

                // Only try alternative if ordinary failure (not labeled failure from T())
                if (!parser->success && !parser->throw_label) {
                  parser->success = true;
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Block", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Block", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_IfStatement(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "IfStatement", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 5 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_checkstack(parser, 1);
        lua_pushlstring(parser->L, "if", 2);
      }
      if (parser->success) {
        { // Match literal "if"
          if (parser->pos + 2 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "if", 2) == 0) {
            parser->pos += 2;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message, "Expected `"
                                           "if"
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
            parse_Value(parser);
            if (parser->success) {
              parse_InBlock(parser);
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "IfStatement", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "IfStatement", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_InBlock(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "InBlock", start);
#endif

  { // Sequence with 5 patterns
    REMEMBER_POSITION(parser, pos);

    parse_nl(parser);
    if (parser->success) {
      { // Zero or more repetitions
        while (true) {
          parse_BlankLine(parser);
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
        { // Indenter advance (stack 0): push width if deeper than top, consume nothing
          size_t ind_end;
          int ind_width = pgen_ind_measure(parser, &ind_end, 2);
          (void)ind_end;
          PgenIndStack *ind_s = &parser->ind_stacks[0];
          if (ind_s->size > 0 && ind_width > ind_s->items[ind_s->size - 1]) {
            pgen_ind_push(parser, 0, ind_width);
          } else {
            parser->success = false;
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "InBlock", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "InBlock", parser->pos);
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
      int ind_width = pgen_ind_measure(parser, &ind_end, 2);
      PgenIndStack *ind_s = &parser->ind_stacks[0];
      if (ind_s->size > 0 && ind_s->items[ind_s->size - 1] == ind_width) {
        parser->pos = ind_end;
      } else {
        parser->success = false;
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Indent width %d does not match current level at position %zu", ind_width, parser->pos);
#endif
      }
    }
    if (parser->success) {
      parse_Statement(parser);
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

static bool parse_Name(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Name", start);
#endif

  { // Capture Table (array-only)
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_checkstack(parser, 1);
        lua_pushlstring(parser->L, "name", 4);
      }
      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          parse_ident(parser);

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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Name", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Name", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_Number(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Number", start);
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
          { // Sequence with 2 patterns
            REMEMBER_POSITION(parser, pos);

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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Number", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Number", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_Statement(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Statement", start);
#endif

  {   // Choice
    { // Choice
      parse_IfStatement(parser);

      // Only try alternative if ordinary failure (not labeled failure from T())
      if (!parser->success && !parser->throw_label) {
        parser->success = true;
        parse_Assign(parser);
      }
    }

    // Only try alternative if ordinary failure (not labeled failure from T())
    if (!parser->success && !parser->throw_label) {
      parser->success = true;
      parse_Value(parser);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Statement", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Statement", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_String(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "String", start);
#endif

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
        {   // Choice
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
              { // Capture
                size_t start_pos = parser->pos;
                { // Zero or more repetitions
                  while (true) {
                    {   // Choice
                      { // Match literal "\\\""
                        if (parser->pos + 2 <= parser->input_len &&
                            memcmp(parser->input + parser->pos, "\\\"", 2) == 0) {
                          parser->pos += 2;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message, "Expected `"
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
                        { // Sequence with 2 patterns
                          REMEMBER_POSITION(parser, pos);

                          { // Negate (only match if pattern fails)
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
                              // Pattern matched, so negate fails
                              RESTORE_POSITION(parser, pos);
                              parser->success = false;
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
                              }
                            }
                            if (!parser->success) {
                              RESTORE_POSITION(parser, pos);
                            }
                          }
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
                      {   // Choice
                        { // Match literal "\\'"
                          if (parser->pos + 2 <= parser->input_len &&
                              memcmp(parser->input + parser->pos, "\\'", 2) == 0) {
                            parser->pos += 2;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message, "Expected `"
                                                           "\\'"
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
                                }
                              }
                              if (!parser->success) {
                                RESTORE_POSITION(parser, pos);
                              }
                            }
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "String", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "String", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_Value(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Value", start);
#endif

  {   // Choice
    { // Choice
      parse_Number(parser);

      // Only try alternative if ordinary failure (not labeled failure from T())
      if (!parser->success && !parser->throw_label) {
        parser->success = true;
        parse_String(parser);
      }
    }

    // Only try alternative if ordinary failure (not labeled failure from T())
    if (!parser->success && !parser->throw_label) {
      parser->success = true;
      parse_Name(parser);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Value", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Value", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

static bool parse_ident(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "ident", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match character range: "az,AZ,__"
      if (parser->pos < parser->input_len &&
          ((parser->input[parser->pos] >= 97 && parser->input[parser->pos] <= 122) || (parser->input[parser->pos] >= 65 && parser->input[parser->pos] <= 90) || (parser->input[parser->pos] >= 95 && parser->input[parser->pos] <= 95))) {
        parser->pos++;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message, "Expected character in ranges ["
                                       "a"
                                       " - "
                                       "z"
                                       ", "
                                       "A"
                                       " - "
                                       "Z"
                                       ", "
                                       "_"
                                       " - "
                                       "_"
                                       "] at position %zu",
                parser->pos);
#endif
        parser->success = false;
      }
    }
    if (parser->success) {
      { // Zero or more repetitions
        while (true) {
          { // Match character range: "az,AZ,09,__"
            if (parser->pos < parser->input_len &&
                ((parser->input[parser->pos] >= 97 && parser->input[parser->pos] <= 122) || (parser->input[parser->pos] >= 65 && parser->input[parser->pos] <= 90) || (parser->input[parser->pos] >= 48 && parser->input[parser->pos] <= 57) || (parser->input[parser->pos] >= 95 && parser->input[parser->pos] <= 95))) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected character in ranges ["
                                             "a"
                                             " - "
                                             "z"
                                             ", "
                                             "A"
                                             " - "
                                             "Z"
                                             ", "
                                             "0"
                                             " - "
                                             "9"
                                             ", "
                                             "_"
                                             " - "
                                             "_"
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "ident", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "ident", parser->pos);
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
static Parser *moonscript_init(const char *input, lua_State *L) {
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

  // Initialize indenter stacks (each seeded with its initial value)
  static const int pgen_ind_initials[PGEN_IND_STACK_COUNT] = {0};
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
static void moonscript_free(Parser *parser) {
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
static int l_moonscript_parse(lua_State *L) {
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
  Parser *parser = moonscript_init(input, L);
  if (!parser) {
    lua_pushnil(L);
    lua_pushstring(L, "Parser initialization failed (memory allocation?)");
    return 2;
  }

  int initial_stack_size = lua_gettop(parser->L);

  parse_File(parser);

  int final_stack_size = lua_gettop(parser->L);

  // Return nil and error info on failure
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size && "Unexpected stack size change on parse failure.");
    lua_pushnil(L);
    if (parser->throw_label) {
      // Labeled failure: return nil, label, position
      lua_pushstring(L, parser->throw_label);
      lua_pushinteger(L, parser->throw_pos + 1); // 1-indexed for Lua
      moonscript_free(parser);
      return 3;
    } else {
      // Ordinary failure: return nil, error_message
      lua_pushstring(L, parser->error_message);
      moonscript_free(parser);
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
    moonscript_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  moonscript_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg moonscript_module[] = {
    {"parse", l_moonscript_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}                   // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_moonscript(lua_State *L) {

  luaL_newlib(L, moonscript_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register
int luaopen_moonscript(lua_State *L) {

  luaL_register(L, "moonscript", moonscript_module); // Registers functions in global table (or package table)
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o moonscript.so -fPIC moonscript.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local moonscript = require "moonscript"
local result = moonscript.parse("your input string")
*/

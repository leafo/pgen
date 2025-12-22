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

typedef struct {
  int tab_width;
  bool allow_tabs;
  bool allow_mixed;
  size_t stack_len;
  size_t stack_cap;
  int *stack;
} LayoutState;

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
  LayoutState layout_states[1];

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

static int layout_top(LayoutState *state) {
  return state->stack_len > 0 ? state->stack[state->stack_len - 1] : 0;
}

static bool layout_push(LayoutState *state, int indent) {
  if (state->stack_len >= state->stack_cap) {
    size_t new_cap = state->stack_cap ? state->stack_cap * 2 : 8;
    int *new_stack = (int *)realloc(state->stack, sizeof(int) * new_cap);
    if (!new_stack) {
      return false;
    }
    state->stack = new_stack;
    state->stack_cap = new_cap;
  }
  state->stack[state->stack_len++] = indent;
  return true;
}

static void layout_state_init(LayoutState *state, int tab_width, bool allow_tabs, bool allow_mixed) {
  state->tab_width = tab_width;
  state->allow_tabs = allow_tabs;
  state->allow_mixed = allow_mixed;
  state->stack_len = 1;
  state->stack_cap = 8;
  state->stack = (int *)malloc(sizeof(int) * state->stack_cap);
  if (state->stack) {
    state->stack[0] = 0;
  } else {
    state->stack_len = 0;
    state->stack_cap = 0;
  }
}

static void layout_state_free(LayoutState *state) {
  if (state->stack) {
    free(state->stack);
  }
  state->stack = NULL;
  state->stack_len = 0;
  state->stack_cap = 0;
}

static bool layout_is_bol(Parser *parser) {
  return parser->pos == 0 || parser->input[parser->pos - 1] == '\n';
}

static bool layout_compute_indent(Parser *parser, LayoutState *state, size_t *indent_out, size_t *pos_out, bool *blank_out) {
  if (!layout_is_bol(parser)) {
    return false;
  }

  size_t pos = parser->pos;
  size_t indent = 0;
  bool saw_space = false;
  bool saw_tab = false;

  while (pos < parser->input_len) {
    char c = parser->input[pos];
    if (c == ' ') {
      indent += 1;
      saw_space = true;
      pos++;
      continue;
    }
    if (c == '\t') {
      if (!state->allow_tabs) {
        return false;
      }
      indent += state->tab_width;
      saw_tab = true;
      pos++;
      continue;
    }
    break;
  }

  if (!state->allow_mixed && saw_space && saw_tab) {
    return false;
  }

  if (pos < parser->input_len && parser->input[pos] == '\n') {
    *blank_out = true;
    *pos_out = pos + 1;
    *indent_out = indent;
    return true;
  }

  *blank_out = false;
  *pos_out = pos;
  *indent_out = indent;
  return true;
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
static bool parse_Dedent(Parser *parser);
static bool parse_IfStatement(Parser *parser);
static bool parse_InBlock(Parser *parser);
static bool parse_Indent(Parser *parser);
static bool parse_Line(Parser *parser);
static bool parse_Name(Parser *parser);
static bool parse_Number(Parser *parser);
static bool parse_SameIndent(Parser *parser);
static bool parse_Statement(Parser *parser);
static bool parse_String(Parser *parser);
static bool parse_Value(Parser *parser);
static bool parse_ident(Parser *parser);
static bool parse_nl(Parser *parser);
static bool parse_ws(Parser *parser);

// Rule functions
static bool parse_File(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "File", start);
#endif

  { // Sequence with 5 patterns
    REMEMBER_POSITION(parser, pos);

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
      parse_Block(parser);
      if (parser->success) {
        { // Zero or more repetitions
          while (true) {
            parse_nl(parser);
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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_Assign(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Assign", start);
#endif

  { // Capture Table (array-only)
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 6 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_BlankLine(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "BlankLine", start);
#endif

  { // Match blank line (layout BlankLine)
    size_t indent = 0;
    size_t new_pos = 0;
    bool is_blank = false;
    LayoutState *layout = &parser->layout_states[0];
    if (layout_compute_indent(parser, layout, &indent, &new_pos, &is_blank) && is_blank) {
      parser->pos = new_pos;
      parser->success = true;
    } else {
      parser->success = false;
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "BlankLine", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "BlankLine", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_Block(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Block", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        lua_pushlstring(parser->L, "block", 5);
      }
      if (parser->success) {
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
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Block", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Block", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_Dedent(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Dedent", start);
#endif

  { // Pop indentation (layout Dedent)
    LayoutState *layout = &parser->layout_states[0];
    if (layout->stack_len > 1) {
      layout->stack_len -= 1;
    }
    parser->success = true;
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Dedent", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Dedent", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_IfStatement(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "IfStatement", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 5 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_InBlock(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "InBlock", start);
#endif

  { // Sequence with 4 patterns
    REMEMBER_POSITION(parser, pos);

    parse_nl(parser);
    if (parser->success) {
      parse_Indent(parser);
      if (parser->success) {
        parse_Block(parser);
        if (parser->success) {
          parse_Dedent(parser);
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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_Indent(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Indent", start);
#endif

  { // Match increased indentation (layout Indent)
    size_t indent = 0;
    size_t new_pos = 0;
    bool is_blank = false;
    LayoutState *layout = &parser->layout_states[0];
    if (layout_compute_indent(parser, layout, &indent, &new_pos, &is_blank) && !is_blank &&
        indent > (size_t)layout_top(layout) && layout_push(layout, (int)indent)) {
      parser->success = true;
    } else {
      parser->success = false;
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "Indent", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "Indent", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_Line(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Line", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    parse_SameIndent(parser);
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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_Name(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Name", start);
#endif

  { // Capture Table (array-only)
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        lua_pushlstring(parser->L, "name", 4);
      }
      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          parse_ident(parser);

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
      int array_count = new_stack_size - initial_stack_size;

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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_Number(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "Number", start);
#endif

  { // Capture Table (array-only)
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
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
      int array_count = new_stack_size - initial_stack_size;

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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_SameIndent(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "SameIndent", start);
#endif

  { // Match same indentation (layout SameIndent)
    size_t indent = 0;
    size_t new_pos = 0;
    bool is_blank = false;
    LayoutState *layout = &parser->layout_states[0];
    if (layout_compute_indent(parser, layout, &indent, &new_pos, &is_blank) && !is_blank &&
        indent == (size_t)layout_top(layout)) {
      parser->pos = new_pos;
      parser->success = true;
    } else {
      parser->success = false;
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "SameIndent", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "SameIndent", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_Statement(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_String(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "String", start);
#endif

  { // Capture Table (array-only)
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
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
                  // TODO: ensure stack has enough space for push
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
                    // TODO: ensure stack has enough space for push
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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_Value(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_ident(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
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
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_nl(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "nl", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match newline (layout NL)
      if (parser->pos < parser->input_len &&
          parser->input[parser->pos] == '\n') {
        parser->pos++;
      } else {
        parser->success = false;
      }
    }
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
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "nl", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "nl", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_ws(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
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
  parser->depth -= 1;
#endif

  return parser->success;
}

// Free parser (forward declaration)
static void moonscript_free(Parser *parser);

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
  layout_state_init(&parser->layout_states[0], 2, true, true);
  if (!parser->layout_states[0].stack) {
    moonscript_free(parser);
    return NULL;
  }
  return parser;
}

// Free parser
static void moonscript_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    layout_state_free(&parser->layout_states[0]);
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

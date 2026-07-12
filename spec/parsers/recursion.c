#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// recursion - generated parser

// Maximum rule-call recursion depth before the parse is aborted with a Lua
// error (prevents C stack overflow on deeply nested input). Override with
// the max_depth compile option or -DPGEN_MAX_DEPTH=n
#ifndef PGEN_MAX_DEPTH
#define PGEN_MAX_DEPTH 5000
#endif

// --- Capture log ---
// Captures are recorded as log entries during matching and only materialized
// into Lua values after the whole parse succeeds. Backtracking rewinds the
// log length, so discarded speculative captures never touch the Lua runtime.
// The exception is Cmt: its callback runs mid-parse and its extra return
// values live on the Lua stack, referenced by PGEN_CAP_VALUE entries.
enum {
  PGEN_CAP_STR,   // start/len: slice of the input
  PGEN_CAP_CONST, // aux: registry ref of an interned constant
  PGEN_CAP_NIL,
  PGEN_CAP_POS,      // start: input position
  PGEN_CAP_VALUE,    // aux: absolute Lua stack index (Cmt results)
  PGEN_CAP_TBL_OPEN, // Ct brackets
  PGEN_CAP_TBL_CLOSE,
  PGEN_CAP_GROUP_OPEN, // Cg brackets; aux: name index, start: input position
  PGEN_CAP_GROUP_CLOSE
};

typedef struct {
  int kind;
  int aux;
  size_t start;
  size_t len;
} PgenCap;

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
  int top;           // Shadow of lua_gettop(L), exact between patterns
  int stack_claimed; // Stack index secured so far via lua_checkstack
  PgenCap *caps;     // Capture log
  size_t cap_len;
  size_t cap_cap;
  lua_State *L;
} Parser;

typedef struct {
  size_t pos;
  size_t cap_len;
  int stack_size;
} ParserPosition;

typedef struct {
  size_t pos;
} ParserInputPosition;

// Set the Lua stack top, keeping the parser's shadow copy in sync. Any
// batched lua_checkstack claim beyond what survives GC stack shrinking is
// forfeited: capacity may shrink to twice the in-use size, but never below
// the runtime's minimum allocation (conservatively PGEN_STACK_FLOOR).
#define PGEN_SETTOP(parser, n)                \
  do {                                        \
    int pgen_newtop_ = (n);                   \
    lua_settop((parser)->L, pgen_newtop_);    \
    (parser)->top = pgen_newtop_;             \
    int pgen_keep_ = 2 * pgen_newtop_;        \
    if (pgen_keep_ < PGEN_STACK_FLOOR)        \
      pgen_keep_ = PGEN_STACK_FLOOR;          \
    if ((parser)->stack_claimed > pgen_keep_) \
      (parser)->stack_claimed = pgen_keep_;   \
  } while (0)

#define REMEMBER_POSITION(parser, pp) \
  ParserPosition pp;                  \
  (pp).pos = (parser)->pos;           \
  (pp).cap_len = (parser)->cap_len;   \
  (pp).stack_size = (parser)->top;

// Restore parser position
#define RESTORE_POSITION(parser, pp) \
  (parser)->pos = (pp).pos;          \
  (parser)->cap_len = (pp).cap_len;  \
  PGEN_SETTOP(parser, (pp).stack_size);

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
//
// Claims are batched so most calls are a single comparison against the
// shadow top. Batch size is limited to what survives GC stack shrinking:
// PUC Lua honors lua_checkstack claims for the frame's lifetime, but
// LuaJIT's GC may shrink capacity to twice the in-use size (never below
// its minimum allocation, conservatively PGEN_STACK_FLOOR). Claims above
// released stack space are forfeited by PGEN_SETTOP.
#define PGEN_STACK_BATCH 64
#define PGEN_STACK_FLOOR 32

static void pgen_checkstack_slow(Parser *parser, int n) {
  int batch = parser->top - n;                            // survives 2x-used shrink
  int floor_batch = PGEN_STACK_FLOOR - (parser->top + n); // under shrink floor
  if (floor_batch > batch)
    batch = floor_batch;
  if (batch > PGEN_STACK_BATCH)
    batch = PGEN_STACK_BATCH;
  if (batch < 0)
    batch = 0;
  if (lua_checkstack(parser->L, n + batch)) {
    parser->stack_claimed = parser->top + n + batch;
  } else if (lua_checkstack(parser->L, n)) {
    // Batched request exceeded the stack limit; the exact one still fits
    parser->stack_claimed = parser->top + n;
  } else {
    luaL_error(parser->L, "pgen: Lua stack overflow while building captures");
  }
}

// Fast path: one comparison against the already-claimed capacity. n may be
// evaluated twice, so call sites must pass side-effect-free expressions.
#define pgen_checkstack(parser, n)                     \
  do {                                                 \
    if ((parser)->top + (n) > (parser)->stack_claimed) \
      pgen_checkstack_slow(parser, n);                 \
  } while (0)

static void pgen_cap_grow(Parser *parser) {
  size_t new_cap = parser->cap_cap * 2;
  PgenCap *caps = (PgenCap *)realloc(parser->caps, new_cap * sizeof(PgenCap));
  if (!caps) {
    luaL_error(parser->L, "pgen: out of memory growing capture log");
  }
  parser->caps = caps;
  parser->cap_cap = new_cap;
}

// Append one log entry. A macro so the hot path (bounds check + four
// stores) inlines into every capture site; arguments may be evaluated
// twice, so call sites must pass side-effect-free expressions.
#define pgen_cap_push(parser, k, a, s, l)                      \
  do {                                                         \
    if ((parser)->cap_len == (parser)->cap_cap)                \
      pgen_cap_grow(parser);                                   \
    PgenCap *pgen_cap_ = &(parser)->caps[(parser)->cap_len++]; \
    pgen_cap_->kind = (k);                                     \
    pgen_cap_->aux = (a);                                      \
    pgen_cap_->start = (s);                                    \
    pgen_cap_->len = (l);                                      \
  } while (0)

// Advance *i past one complete log item (a single entry, or a whole
// bracketed Ct/Cg range including anything nested)
static void pgen_cap_skip(Parser *parser, size_t *i) {
  int kind = parser->caps[*i].kind;
  (*i)++;
  if (kind == PGEN_CAP_TBL_OPEN || kind == PGEN_CAP_GROUP_OPEN) {
    int depth = 1;
    while (depth > 0) {
      kind = parser->caps[*i].kind;
      if (kind == PGEN_CAP_TBL_OPEN || kind == PGEN_CAP_GROUP_OPEN)
        depth++;
      else if (kind == PGEN_CAP_TBL_CLOSE || kind == PGEN_CAP_GROUP_CLOSE)
        depth--;
      (*i)++;
    }
  }
}

// Reduce caps[base..] to only the nth capture value (group captures don't
// count), or to a single nil when there are fewer than n values
static void pgen_cap_select(Parser *parser, size_t base, int n) {
  size_t i = base;
  int count = 0;
  while (i < parser->cap_len) {
    if (parser->caps[i].kind == PGEN_CAP_GROUP_OPEN) {
      pgen_cap_skip(parser, &i);
      continue;
    }
    size_t item_start = i;
    pgen_cap_skip(parser, &i);
    count++;
    if (count == n) {
      size_t item_len = i - item_start;
      memmove(&parser->caps[base], &parser->caps[item_start], item_len * sizeof(PgenCap));
      parser->cap_len = base + item_len;
      return;
    }
  }
  parser->cap_len = base;
  pgen_cap_push(parser, PGEN_CAP_NIL, 0, 0, 0);
}

// Match the text of the most recent visible "name" capture group at the
// current input position. Groups inside completed capture tables are not
// visible, mirroring the previous stack-based behavior where Ct consumed
// its inner captures.
static bool pgen_cap_match_back(Parser *parser, int name_idx) {
  size_t i = parser->cap_len;
  while (i > 0) {
    i--;
    int kind = parser->caps[i].kind;
    if (kind == PGEN_CAP_TBL_CLOSE || kind == PGEN_CAP_GROUP_CLOSE) {
      size_t close = i;
      int depth = 1;
      while (depth > 0) {
        i--;
        int k2 = parser->caps[i].kind;
        if (k2 == PGEN_CAP_TBL_CLOSE || k2 == PGEN_CAP_GROUP_CLOSE)
          depth++;
        else if (k2 == PGEN_CAP_TBL_OPEN || k2 == PGEN_CAP_GROUP_OPEN)
          depth--;
      }
      if (kind == PGEN_CAP_GROUP_CLOSE && parser->caps[i].aux == name_idx) {
        const char *text;
        size_t text_len;
        size_t inner = i + 1;
        if (inner == close) {
          // group captured nothing: its value is the text it matched
          text = parser->input + parser->caps[i].start;
          text_len = parser->caps[close].start - parser->caps[i].start;
        } else if (parser->caps[inner].kind == PGEN_CAP_STR) {
          text = parser->input + parser->caps[inner].start;
          text_len = parser->caps[inner].len;
        } else if (parser->caps[inner].kind == PGEN_CAP_CONST) {
          // interned constant: compare through the materialized value
          bool matched = false;
          pgen_checkstack(parser, 1);
          lua_rawgeti(parser->L, LUA_REGISTRYINDEX, parser->caps[inner].aux);
          if (lua_type(parser->L, -1) == LUA_TSTRING) {
            size_t const_len;
            const char *const_str = lua_tolstring(parser->L, -1, &const_len);
            matched = parser->pos + const_len <= parser->input_len &&
                      memcmp(parser->input + parser->pos, const_str, const_len) == 0;
            if (matched)
              parser->pos += const_len;
          }
          lua_pop(parser->L, 1);
          return matched;
        } else {
          return false; // group holds a non-string value
        }
        if (parser->pos + text_len <= parser->input_len &&
            memcmp(parser->input + parser->pos, text, text_len) == 0) {
          parser->pos += text_len;
          return true;
        }
        return false;
      }
    }
  }
  return false;
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

// No named capture groups
static int __cg_name_refs[1];
// --- Capture log evaluation ---

static void pgen_cap_eval(Parser *parser, size_t *i);

// Push the single value a capture group produces: its first inner capture,
// or the text it matched when it contains no captures
static void pgen_cap_eval_group(Parser *parser, size_t *i) {
  size_t open = *i;
  pgen_cap_skip(parser, i); // *i now points past GROUP_CLOSE
  size_t close = *i - 1;
  if (open + 1 == close) {
    size_t start = parser->caps[open].start;
    pgen_checkstack(parser, 1);
    lua_pushlstring(parser->L, parser->input + start, parser->caps[close].start - start);
    parser->top++;
  } else {
    size_t j = open + 1;
    pgen_cap_eval(parser, &j); // first value only; extras are discarded
  }
}

// Materialize one log item (entry or bracketed range) at *i, pushing
// exactly one Lua value and advancing *i past the item
static void pgen_cap_eval(Parser *parser, size_t *i) {
  PgenCap *cap = &parser->caps[*i];
  switch (cap->kind) {
  case PGEN_CAP_STR:
    pgen_checkstack(parser, 1);
    lua_pushlstring(parser->L, parser->input + cap->start, cap->len);
    parser->top++;
    (*i)++;
    return;
  case PGEN_CAP_CONST:
    pgen_checkstack(parser, 1);
    lua_rawgeti(parser->L, LUA_REGISTRYINDEX, cap->aux);
    parser->top++;
    (*i)++;
    return;
  case PGEN_CAP_NIL:
    pgen_checkstack(parser, 1);
    lua_pushnil(parser->L);
    parser->top++;
    (*i)++;
    return;
  case PGEN_CAP_POS:
    pgen_checkstack(parser, 1);
    lua_pushinteger(parser->L, (lua_Integer)(cap->start + 1));
    parser->top++;
    (*i)++;
    return;
  case PGEN_CAP_VALUE:
    pgen_checkstack(parser, 1);
    lua_pushvalue(parser->L, cap->aux);
    parser->top++;
    (*i)++;
    return;
  case PGEN_CAP_GROUP_OPEN:
    pgen_cap_eval_group(parser, i);
    return;
  default: { // PGEN_CAP_TBL_OPEN
    // No presizing: counting items would re-walk every nested subtree at
    // each nesting level, which costs more than letting the table grow
    pgen_checkstack(parser, 3);
    lua_createtable(parser->L, 0, 0);
    parser->top++;
    int table_idx = parser->top;

    size_t j = *i + 1;
    int array_idx = 1;
    while (parser->caps[j].kind != PGEN_CAP_TBL_CLOSE) {
      if (parser->caps[j].kind == PGEN_CAP_GROUP_OPEN) {
        pgen_checkstack(parser, 2);
        lua_rawgeti(parser->L, LUA_REGISTRYINDEX, __cg_name_refs[parser->caps[j].aux]);
        parser->top++;
        pgen_cap_eval_group(parser, &j);
        lua_rawset(parser->L, table_idx);
        parser->top -= 2;
      } else {
        pgen_cap_eval(parser, &j);
        lua_rawseti(parser->L, table_idx, array_idx++);
        parser->top--;
      }
    }
    *i = j + 1; // past TBL_CLOSE
    return;
  }
  }
}

// Run a match-time capture: materialize the inner captures, call the
// callback with (subject, pos, ...captures), and interpret its results per
// lpeg semantics: position/true = success, false/nil = failure, extra
// return values become captures (kept on the Lua stack)
static void pgen_run_cmt(Parser *parser, int func_ref, size_t start_pos, size_t cap_base, int top_base) {
  lua_State *L = parser->L;
  size_t pos_after_inner = parser->pos;
  int leftovers = parser->top - top_base; // nested Cmt values still on the stack

  pgen_checkstack(parser, 3);
  lua_rawgeti(L, LUA_REGISTRYINDEX, func_ref);
  lua_pushlstring(L, parser->input, parser->input_len);
  lua_pushinteger(L, (lua_Integer)(pos_after_inner + 1)); // 1-based
  parser->top += 3;

  int nargs = 2;
  size_t i = cap_base;
  while (i < parser->cap_len) {
    if (parser->caps[i].kind == PGEN_CAP_GROUP_OPEN) {
      // named groups only matter inside Ct; they aren't passed as arguments
      pgen_cap_skip(parser, &i);
    } else {
      pgen_cap_eval(parser, &i);
      nargs++;
    }
  }
  parser->cap_len = cap_base; // consume the inner captures

  // lua_call propagates errors (aborts parse on Lua error)
  lua_call(L, nargs, LUA_MULTRET);
  parser->top = lua_gettop(L);
  if (parser->stack_claimed > parser->top)
    parser->stack_claimed = parser->top;

  int returns_count = parser->top - (top_base + leftovers);

  if (returns_count == 0) {
    // No return value = match fails
    parser->success = false;
    PGEN_RECORD_FURTHEST(parser); // record at pos_after_inner, before rewind
    parser->pos = start_pos;
  } else {
    int first = top_base + leftovers + 1;
    int first_type = lua_type(L, first);
    if (first_type == LUA_TNUMBER) {
      // Number = new position (1-based from Lua)
      lua_Integer new_pos = lua_tointeger(L, first) - 1;
      // Per lpeg: must be in range [pos_after_inner, input_len]
      if (new_pos >= (lua_Integer)pos_after_inner && new_pos <= (lua_Integer)parser->input_len) {
        parser->pos = (size_t)new_pos;
        parser->success = true;
      } else {
        parser->success = false;
        PGEN_RECORD_FURTHEST(parser);
        parser->pos = start_pos;
      }
    } else if (first_type == LUA_TBOOLEAN && lua_toboolean(L, first)) {
      // true = succeed without consuming (position stays at pos_after_inner)
      parser->success = true;
    } else {
      // false, nil, or other = fail
      parser->success = false;
      PGEN_RECORD_FURTHEST(parser);
      parser->pos = start_pos;
    }
  }

  if (parser->success && returns_count > 1) {
    // Drop leftover nested-Cmt slots and the first return value; the
    // remaining returns stay on the stack as the new captures
    for (int r = 0; r < leftovers + 1; r++) {
      lua_remove(L, top_base + 1);
    }
    parser->top -= leftovers + 1;
    if (parser->stack_claimed > parser->top)
      parser->stack_claimed = parser->top;
    int extras = returns_count - 1;
    for (int r = 0; r < extras; r++) {
      pgen_cap_push(parser, PGEN_CAP_VALUE, top_base + 1 + r, 0, 0);
    }
  } else {
    PGEN_SETTOP(parser, top_base);
  }
}

// Forward declarations
static bool parse_expr(Parser *parser);

// Rule functions
static bool parse_expr(Parser *parser) {
  size_t start = parser->pos;

  parser->depth += 1;
  if (parser->depth > PGEN_MAX_DEPTH) {
    // A Lua error (rather than a match failure) so the overflow can't be
    // silently converted into a successful parse by a predicate or choice
    luaL_error(parser->L, "pgen: max recursion depth (%d) exceeded at position %d", (int)PGEN_MAX_DEPTH, (int)(parser->pos + 1));
  }

#ifdef PGEN_DEBUG
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth, "", "expr", start);
#endif

  {   // Choice
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Match single character "("
        if (parser->pos < parser->input_len &&
            parser->input[parser->pos] == 40) {
          parser->pos++;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message, "Expected character `"
                                         "("
                                         "` at position %zu",
                  parser->pos);
#endif
          parser->success = false;
        }
      }
      if (parser->success) {
        parse_expr(parser);
        if (parser->success) {
          { // Match single character ")"
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 41) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message, "Expected character `"
                                             ")"
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
          pgen_cap_push(parser, PGEN_CAP_STR, 0, start_pos, parser->pos - start_pos);
        }
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth, "", "expr", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "", (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth, "", "expr", parser->pos);
  }
#endif

  parser->depth -= 1;
  return parser->success;
}

// Initialize parser
static Parser *recursion_init(const char *input, lua_State *L) {
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
  parser->top = lua_gettop(L);
  parser->stack_claimed = parser->top;
  parser->caps = (PgenCap *)malloc(64 * sizeof(PgenCap));
  if (!parser->caps) {
    free(parser);
    return NULL;
  }
  parser->cap_len = 0;
  parser->cap_cap = 64;
  parser->L = L;
  return parser;
}

// Free parser
static void recursion_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser->caps);
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_recursion_parse(lua_State *L) {
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
  Parser *parser = recursion_init(input, L);
  if (!parser) {
    lua_pushnil(L);
    lua_pushstring(L, "Parser initialization failed (memory allocation?)");
    return 2;
  }

  int initial_stack_size = lua_gettop(parser->L);

  parse_expr(parser);

  int final_stack_size = lua_gettop(parser->L);
  assert(parser->top == final_stack_size && "Shadow stack top out of sync.");

  // Return nil and error info on failure
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size && "Unexpected stack size change on parse failure.");
    assert(parser->cap_len == 0 && "Capture log not empty on parse failure.");
    lua_pushnil(L);
    if (parser->throw_label) {
      // Labeled failure: return nil, label, position
      lua_pushstring(L, parser->throw_label);
      lua_pushinteger(L, parser->throw_pos + 1); // 1-indexed for Lua
      recursion_free(parser);
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
      recursion_free(parser);
      return 3;
    }
  }

  // Materialize the capture log into return values. Named groups produce
  // no top-level values (they only matter inside Ct).
  int cmt_slots = parser->top - initial_stack_size; // lingering Cmt values
  int result_count = 0;
  size_t cap_i = 0;
  while (cap_i < parser->cap_len) {
    if (parser->caps[cap_i].kind == PGEN_CAP_GROUP_OPEN) {
      pgen_cap_skip(parser, &cap_i);
    } else {
      pgen_cap_eval(parser, &cap_i);
      result_count++;
    }
  }

  // Drop the lingering Cmt value slots sitting beneath the results
  for (int i = 0; i < cmt_slots; i++) {
    lua_remove(L, initial_stack_size + 1);
  }

  if (result_count > 0) {
    recursion_free(parser);
    return result_count;
  }

  // Success case with no captures
  lua_pushinteger(L, parser->pos + 1);
  recursion_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg recursion_module[] = {
    {"parse", l_recursion_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}                  // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_recursion(lua_State *L) {

  luaL_newlib(L, recursion_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register. Register into a fresh table rather than a
// named global: a name would be shared through package.loaded, so loading
// two parsers compiled with the same parser_name in one process would
// silently overwrite the first module's parse function.
int luaopen_recursion(lua_State *L) {

  lua_newtable(L);
  luaL_register(L, NULL, recursion_module);
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o recursion.so -fPIC recursion.c `pkg-config --cflags --libs lua5.1`

To use in Lua:
local recursion = require "recursion"
local result = recursion.parse("your input string")
*/

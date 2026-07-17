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
  PGEN_CAP_GROUP_CLOSE,
  PGEN_CAP_FN_OPEN, // Cfn brackets; aux: callback registry ref, start: pos
  PGEN_CAP_FN_CLOSE
};

// Bracket kind tests: OPEN kinds and their CLOSE kinds are laid out in
// matching order after the scalar kinds
#define PGEN_CAP_IS_OPEN(k) \
  ((k) == PGEN_CAP_TBL_OPEN || (k) == PGEN_CAP_GROUP_OPEN || (k) == PGEN_CAP_FN_OPEN)
#define PGEN_CAP_IS_CLOSE(k) \
  ((k) == PGEN_CAP_TBL_CLOSE || (k) == PGEN_CAP_GROUP_CLOSE || (k) == PGEN_CAP_FN_CLOSE)

typedef struct {
  int kind;
  int aux;
  size_t start;
  size_t len;
} PgenCap;

// Single-slot memo for position-pure rules: pos is the memoized input
// position + 1 (0 = empty slot), endpos the resulting position or
// (size_t)-1 for failure
#define PGEN_MEMO_COUNT 4
typedef struct {
  size_t pos;
  size_t endpos;
} PgenMemoSlot;

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
  size_t furthest_fail;    // Furthest position where a match attempt failed
  size_t depth;
  int top;           // Shadow of lua_gettop(L), exact between patterns
  int stack_claimed; // Stack index secured so far via lua_checkstack
  PgenCap *caps;     // Capture log
  size_t cap_len;
  size_t cap_cap;
  PgenMemoSlot memo[PGEN_MEMO_COUNT];
  lua_State *L;
  PgenIndStack ind_stacks[PGEN_IND_STACK_COUNT];
  PgenTrailEntry *trail;
  size_t trail_len;
  size_t trail_cap;
} Parser;

typedef struct {
  size_t pos;
  size_t cap_len;
  int stack_size;
  size_t trail_index;
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
  (pp).stack_size = (parser)->top;    \
  (pp).trail_index = (parser)->trail_len;

// Restore parser position
#define RESTORE_POSITION(parser, pp)    \
  (parser)->pos = (pp).pos;             \
  (parser)->cap_len = (pp).cap_len;     \
  PGEN_SETTOP(parser, (pp).stack_size); \
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
  if (PGEN_CAP_IS_OPEN(kind)) {
    int depth = 1;
    while (depth > 0) {
      kind = parser->caps[*i].kind;
      if (PGEN_CAP_IS_OPEN(kind))
        depth++;
      else if (PGEN_CAP_IS_CLOSE(kind))
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
    if (PGEN_CAP_IS_CLOSE(kind)) {
      size_t close = i;
      int depth = 1;
      while (depth > 0) {
        i--;
        int k2 = parser->caps[i].kind;
        if (PGEN_CAP_IS_CLOSE(k2))
          depth++;
        else if (PGEN_CAP_IS_OPEN(k2))
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

// No named capture groups
static int __cg_name_refs[1];
// Interned constants (pushed once at module load)
static int __const_refs[6];

static void __const_init(lua_State *L) {
  lua_pushlstring(L, "assign", 6);
  __const_refs[0] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "block", 5);
  __const_refs[1] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "if", 2);
  __const_refs[2] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "name", 4);
  __const_refs[3] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "number", 6);
  __const_refs[4] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushlstring(L, "string", 6);
  __const_refs[5] = luaL_ref(L, LUA_REGISTRYINDEX);
}
// --- Capture log evaluation ---

static int pgen_cap_eval(Parser *parser, size_t *i);

// Push the single value a capture group produces: its first inner capture
// value, or the text it matched when its contents produce no values
static void pgen_cap_eval_group(Parser *parser, size_t *i) {
  size_t open = *i;
  pgen_cap_skip(parser, i); // *i now points past GROUP_CLOSE
  size_t close = *i - 1;

  size_t j = open + 1;
  while (j < close) {
    int produced = pgen_cap_eval(parser, &j);
    if (produced > 0) {
      if (produced > 1) {
        // keep only the first value
        lua_pop(parser->L, produced - 1);
        parser->top -= produced - 1;
      }
      return;
    }
  }

  // no values: the group's value is the text it matched
  size_t start = parser->caps[open].start;
  pgen_checkstack(parser, 1);
  lua_pushlstring(parser->L, parser->input + start, parser->caps[close].start - start);
  parser->top++;
}

// Materialize one log item (entry or bracketed range) at *i, advancing *i
// past the item. Returns the number of Lua values pushed: always 1 except
// for transform captures, whose callbacks may return any number of values.
static int pgen_cap_eval(Parser *parser, size_t *i) {
  PgenCap *cap = &parser->caps[*i];
  switch (cap->kind) {
  case PGEN_CAP_STR:
    pgen_checkstack(parser, 1);
    lua_pushlstring(parser->L, parser->input + cap->start, cap->len);
    parser->top++;
    (*i)++;
    return 1;
  case PGEN_CAP_CONST:
    pgen_checkstack(parser, 1);
    lua_rawgeti(parser->L, LUA_REGISTRYINDEX, cap->aux);
    parser->top++;
    (*i)++;
    return 1;
  case PGEN_CAP_NIL:
    pgen_checkstack(parser, 1);
    lua_pushnil(parser->L);
    parser->top++;
    (*i)++;
    return 1;
  case PGEN_CAP_POS:
    pgen_checkstack(parser, 1);
    lua_pushinteger(parser->L, (lua_Integer)(cap->start + 1));
    parser->top++;
    (*i)++;
    return 1;
  case PGEN_CAP_VALUE:
    pgen_checkstack(parser, 1);
    lua_pushvalue(parser->L, cap->aux);
    parser->top++;
    (*i)++;
    return 1;
  case PGEN_CAP_GROUP_OPEN:
    pgen_cap_eval_group(parser, i);
    return 1;
  case PGEN_CAP_FN_OPEN: {
    // Transform capture: inner values become arguments, the callback's
    // return values become the capture values (innermost-first order falls
    // out of the recursion here)
    size_t open = *i;
    int func_base = parser->top;
    pgen_checkstack(parser, 1);
    lua_rawgeti(parser->L, LUA_REGISTRYINDEX, cap->aux);
    parser->top++;

    int nargs = 0;
    size_t j = open + 1;
    while (parser->caps[j].kind != PGEN_CAP_FN_CLOSE) {
      if (parser->caps[j].kind == PGEN_CAP_GROUP_OPEN) {
        // named groups are not visible as arguments (as at the top level)
        pgen_cap_skip(parser, &j);
      } else {
        nargs += pgen_cap_eval(parser, &j);
      }
    }

    if (nargs == 0) {
      // no inner captures: the callback receives the matched text
      size_t start = parser->caps[open].start;
      pgen_checkstack(parser, 1);
      lua_pushlstring(parser->L, parser->input + start, parser->caps[j].start - start);
      nargs = 1;
    }

    // lua_call propagates errors (aborts materialization on Lua error)
    lua_call(parser->L, nargs, LUA_MULTRET);
    parser->top = lua_gettop(parser->L);
    if (parser->stack_claimed > parser->top)
      parser->stack_claimed = parser->top;

    *i = j + 1; // past FN_CLOSE
    return parser->top - func_base;
  }
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
        // rawseti pops the top value, so multi-value items assign their
        // indexes in reverse
        int produced = pgen_cap_eval(parser, &j);
        for (int v = produced - 1; v >= 0; v--) {
          lua_rawseti(parser->L, table_idx, array_idx + v);
        }
        array_idx += produced;
        parser->top -= produced;
      }
    }
    *i = j + 1; // past TBL_CLOSE
    return 1;
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
      nargs += pgen_cap_eval(parser, &i);
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

  { // Capture Table
    size_t ct_cap_start = parser->cap_len;
    pgen_cap_push(parser, PGEN_CAP_TBL_OPEN, 0, 0, 0);
    { // Sequence with 6 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_cap_push(parser, PGEN_CAP_CONST, __const_refs[0], 0, 0); // "assign"
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
      pgen_cap_push(parser, PGEN_CAP_TBL_CLOSE, 0, 0, 0);
    } else {
      parser->cap_len = ct_cap_start;
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
  // Position-pure rule (no captures, labels, or other state): a
  // single-slot memo short-circuits the repeated calls that backtracking
  // alternatives make at the same position
  if (parser->memo[0].pos == start + 1) {
    if (parser->memo[0].endpos == (size_t)-1) {
      parser->success = false;
      return false;
    }
    parser->pos = parser->memo[0].endpos;
    parser->success = true;
    return true;
  }

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
    REMEMBER_INPUT_POSITION(parser, pos);

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
        RESTORE_INPUT_POSITION(parser, pos);
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
  parser->memo[0].pos = start + 1;
  parser->memo[0].endpos = parser->success ? parser->pos : (size_t)-1;

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
    size_t ct_cap_start = parser->cap_len;
    pgen_cap_push(parser, PGEN_CAP_TBL_OPEN, 0, 0, 0);
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_cap_push(parser, PGEN_CAP_CONST, __const_refs[1], 0, 0); // "block"
      }
      if (parser->success) {
        { // At least 1 repetitions
          REMEMBER_POSITION(parser, pos);
          size_t rep_count = 0;

          while (true) {
            { // Choice
              parse_BlankLine(parser);

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
      pgen_cap_push(parser, PGEN_CAP_TBL_CLOSE, 0, 0, 0);
    } else {
      parser->cap_len = ct_cap_start;
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
    size_t ct_cap_start = parser->cap_len;
    pgen_cap_push(parser, PGEN_CAP_TBL_OPEN, 0, 0, 0);
    { // Sequence with 5 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_cap_push(parser, PGEN_CAP_CONST, __const_refs[2], 0, 0); // "if"
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
            PGEN_RECORD_FURTHEST(parser);
          }
        }
        if (parser->success) {
          { // At least 1 repetitions
            REMEMBER_INPUT_POSITION(parser, pos);
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
              RESTORE_INPUT_POSITION(parser, pos);
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
      pgen_cap_push(parser, PGEN_CAP_TBL_CLOSE, 0, 0, 0);
    } else {
      parser->cap_len = ct_cap_start;
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
        PGEN_RECORD_FURTHEST(parser);
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

  { // Capture Table
    size_t ct_cap_start = parser->cap_len;
    pgen_cap_push(parser, PGEN_CAP_TBL_OPEN, 0, 0, 0);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_cap_push(parser, PGEN_CAP_CONST, __const_refs[3], 0, 0); // "name"
      }
      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          parse_ident(parser);

          if (parser->success) {
            pgen_cap_push(parser, PGEN_CAP_STR, 0, start_pos, parser->pos - start_pos);
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      pgen_cap_push(parser, PGEN_CAP_TBL_CLOSE, 0, 0, 0);
    } else {
      parser->cap_len = ct_cap_start;
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

  { // Capture Table
    size_t ct_cap_start = parser->cap_len;
    pgen_cap_push(parser, PGEN_CAP_TBL_OPEN, 0, 0, 0);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_cap_push(parser, PGEN_CAP_CONST, __const_refs[4], 0, 0); // "number"
      }
      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          { // Sequence with 2 patterns
            REMEMBER_INPUT_POSITION(parser, pos);

            { // At least 1 repetitions
              REMEMBER_INPUT_POSITION(parser, pos);
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
                RESTORE_INPUT_POSITION(parser, pos);
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
                      REMEMBER_INPUT_POSITION(parser, pos);

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
                          REMEMBER_INPUT_POSITION(parser, pos);
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
                            RESTORE_INPUT_POSITION(parser, pos);
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message, "Expected 1 repetitions at position %zu", parser->pos);
#endif
                          }
                        }
                        if (!parser->success) {
                          RESTORE_INPUT_POSITION(parser, pos);
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
                RESTORE_INPUT_POSITION(parser, pos);
              }
            }
          }

          if (parser->success) {
            pgen_cap_push(parser, PGEN_CAP_STR, 0, start_pos, parser->pos - start_pos);
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      pgen_cap_push(parser, PGEN_CAP_TBL_CLOSE, 0, 0, 0);
    } else {
      parser->cap_len = ct_cap_start;
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

      if (!parser->success && !parser->throw_label) {
        parser->success = true;
        parse_Assign(parser);
      }
    }

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

  { // Capture Table
    size_t ct_cap_start = parser->cap_len;
    pgen_cap_push(parser, PGEN_CAP_TBL_OPEN, 0, 0, 0);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given values
        pgen_cap_push(parser, PGEN_CAP_CONST, __const_refs[5], 0, 0); // "string"
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
                          PGEN_RECORD_FURTHEST(parser);
                        }
                      }

                      if (!parser->success && !parser->throw_label) {
                        parser->success = true;
                        { // Sequence with 2 patterns
                          REMEMBER_INPUT_POSITION(parser, pos);

                          { // Negate (only match if pattern fails)
                            REMEMBER_INPUT_POSITION(parser, pos);

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
                  pgen_cap_push(parser, PGEN_CAP_STR, 0, start_pos, parser->pos - start_pos);
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
                            PGEN_RECORD_FURTHEST(parser);
                          }
                        }

                        if (!parser->success && !parser->throw_label) {
                          parser->success = true;
                          { // Sequence with 2 patterns
                            REMEMBER_INPUT_POSITION(parser, pos);

                            { // Negate (only match if pattern fails)
                              REMEMBER_INPUT_POSITION(parser, pos);

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
                    pgen_cap_push(parser, PGEN_CAP_STR, 0, start_pos, parser->pos - start_pos);
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
      pgen_cap_push(parser, PGEN_CAP_TBL_CLOSE, 0, 0, 0);
    } else {
      parser->cap_len = ct_cap_start;
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

      if (!parser->success && !parser->throw_label) {
        parser->success = true;
        parse_String(parser);
      }
    }

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
  // Position-pure rule (no captures, labels, or other state): a
  // single-slot memo short-circuits the repeated calls that backtracking
  // alternatives make at the same position
  if (parser->memo[1].pos == start + 1) {
    if (parser->memo[1].endpos == (size_t)-1) {
      parser->success = false;
      return false;
    }
    parser->pos = parser->memo[1].endpos;
    parser->success = true;
    return true;
  }

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
    REMEMBER_INPUT_POSITION(parser, pos);

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
        RESTORE_INPUT_POSITION(parser, pos);
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
  parser->memo[1].pos = start + 1;
  parser->memo[1].endpos = parser->success ? parser->pos : (size_t)-1;

  parser->depth -= 1;
  return parser->success;
}

static bool parse_nl(Parser *parser) {
  size_t start = parser->pos;
  // Position-pure rule (no captures, labels, or other state): a
  // single-slot memo short-circuits the repeated calls that backtracking
  // alternatives make at the same position
  if (parser->memo[2].pos == start + 1) {
    if (parser->memo[2].endpos == (size_t)-1) {
      parser->success = false;
      return false;
    }
    parser->pos = parser->memo[2].endpos;
    parser->success = true;
    return true;
  }

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
  parser->memo[2].pos = start + 1;
  parser->memo[2].endpos = parser->success ? parser->pos : (size_t)-1;

  parser->depth -= 1;
  return parser->success;
}

static bool parse_ws(Parser *parser) {
  size_t start = parser->pos;
  // Position-pure rule (no captures, labels, or other state): a
  // single-slot memo short-circuits the repeated calls that backtracking
  // alternatives make at the same position
  if (parser->memo[3].pos == start + 1) {
    if (parser->memo[3].endpos == (size_t)-1) {
      parser->success = false;
      return false;
    }
    parser->pos = parser->memo[3].endpos;
    parser->success = true;
    return true;
  }

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
  parser->memo[3].pos = start + 1;
  parser->memo[3].endpos = parser->success ? parser->pos : (size_t)-1;

  parser->depth -= 1;
  return parser->success;
}

#define PGEN_PARSER_MT "pgen.moonscript"

// Initialize a parser anchored in a Lua userdata (left on the stack). Its
// metatable's __gc frees the owned allocations, so a Lua error unwinding
// out of a parse (transform/Cmt callbacks, recursion depth, out of memory)
// cannot leak them.
static Parser *moonscript_init(const char *input, lua_State *L) {
  Parser *parser = (Parser *)lua_newuserdata(L, sizeof(Parser));

  // Null the owned pointers before attaching the metatable so __gc is
  // safe even if a later allocation fails mid-init
  parser->caps = NULL;
  parser->trail = NULL;
  parser->trail_len = 0;
  parser->trail_cap = 0;
  for (int i = 0; i < PGEN_IND_STACK_COUNT; i++) {
    parser->ind_stacks[i].items = NULL;
  }
  luaL_getmetatable(L, PGEN_PARSER_MT);
  lua_setmetatable(L, -2);

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
  parser->L = L;

  parser->caps = (PgenCap *)malloc(64 * sizeof(PgenCap));
  if (!parser->caps) {
    luaL_error(L, "pgen: out of memory initializing parser");
  }
  parser->cap_len = 0;
  parser->cap_cap = 64;
  for (int i = 0; i < PGEN_MEMO_COUNT; i++) {
    parser->memo[i].pos = 0; // empty slot
  }

  // Initialize indenter stacks (each starts holding its initial value)
  static const int pgen_ind_initials[PGEN_IND_STACK_COUNT] = {0};
  for (int i = 0; i < PGEN_IND_STACK_COUNT; i++) {
    parser->ind_stacks[i].items = (int *)malloc(8 * sizeof(int));
    if (!parser->ind_stacks[i].items) {
      luaL_error(L, "pgen: out of memory initializing parser");
    }
    parser->ind_stacks[i].cap = 8;
    parser->ind_stacks[i].size = 1;
    parser->ind_stacks[i].items[0] = pgen_ind_initials[i];
  }

  return parser;
}

// Free the parser's owned allocations. Idempotent: called eagerly on
// normal completion and again from __gc, which also covers error unwinds
static void moonscript_free(Parser *parser) {
  if (parser) {
    for (int i = 0; i < PGEN_IND_STACK_COUNT; i++) {
      free(parser->ind_stacks[i].items);
      parser->ind_stacks[i].items = NULL;
    }
    free(parser->trail);
    parser->trail = NULL;
    free(parser->caps);
    parser->caps = NULL;
  }
}

// --- Lua Module Interface ---

// __gc for the parser userdata: frees whatever the eager free didn't
static int l_moonscript_gc(lua_State *L) {
  moonscript_free((Parser *)lua_touserdata(L, 1));
  return 0;
}

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

  // Initialize the parser (a userdata anchored on the stack; see _init)
  Parser *parser = moonscript_init(input, L);

  int initial_stack_size = lua_gettop(parser->L);

  parse_File(parser);

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
      moonscript_free(parser);
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
      moonscript_free(parser);
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
      result_count += pgen_cap_eval(parser, &cap_i);
    }
  }

  // Drop the lingering Cmt value slots sitting beneath the results
  for (int i = 0; i < cmt_slots; i++) {
    lua_remove(L, initial_stack_size + 1);
  }

  if (result_count > 0) {
    moonscript_free(parser);
    return result_count;
  }

  // Success case with no captures
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
  if (luaL_newmetatable(L, PGEN_PARSER_MT)) {
    lua_pushcfunction(L, l_moonscript_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_pop(L, 1);
  __const_init(L);

  luaL_newlib(L, moonscript_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register. Register into a fresh table rather than a
// named global: a name would be shared through package.loaded, so loading
// two parsers compiled with the same parser_name in one process would
// silently overwrite the first module's parse function.
int luaopen_moonscript(lua_State *L) {
  if (luaL_newmetatable(L, PGEN_PARSER_MT)) {
    lua_pushcfunction(L, l_moonscript_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_pop(L, 1);
  __const_init(L);

  lua_newtable(L);
  luaL_register(L, NULL, moonscript_module);
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

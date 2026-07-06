# Proposal: Indent-based parsing support for pgen (MoonScript port)

Goal: augment pgen so it can express indentation-sensitive grammars like
MoonScript's, then port `moonscript/parse.moon` to a pgen grammar. This
document reviews how the original parser works, evaluates the current
Cmt-based prototype (`spec/parsers/moonscript.lua`), and proposes engine
additions.

## 1. How the MoonScript parser handles indentation

The parser (`moonscript/moonscript/parse.moon` + `parse/util.moon`) is an LPEG
grammar with **mutable Lua-side state** threaded in via `Cmt`:

- **Indent stack** (`_indent`, starts at `{0}`): a stack of indentation widths.
  `Indent = C(S"\t "^0) / count_width` computes a width (space = 1, tab = 4).
- **`CheckIndent`** = `Cmt(Indent, check_indent)`: a line only matches if its
  measured width equals the top of the stack.
- **`Advance`** = `L(Cmt(Indent, advance_indent))`: lookahead (so the
  whitespace is left for the body's `CheckIndent`); pushes the new width if it
  is strictly greater than the top. Refuses to advance when the top is the
  sentinel `-1`.
- **`PushIndent` / `PopIndent`**: unconditional push/pop, used by multi-line
  table literals so that continuation lines may sit at *any* deeper column.
- **`PreventIndent`** = push sentinel `-1`: makes any nested `Advance` fail.
  Used in `class X extends <Exp>` so the expression can't swallow the class
  body block.
- **Block structure**: `Block: Ct(Line * (Break^1 * Line)^0)`,
  `Line: CheckIndent * Statement + Space * L(Stop)`,
  `Body: Break * EmptyLine^0 * InBlock + Ct(Statement)` (inline one-liner or
  indented block), `InBlock: Advance * Block * PopIndent`.

A second stack, **`_do_stack`**, resolves the `do` ambiguity: inside the
header expression of `while`/`with`/`switch`/`for` (which accept an optional
trailing `do` keyword), the `do` *expression* is disabled (`DisableDo` pushes
`false`; `Cmt(Do, check_do)` reads the top; `PopDo` pops).

### The central fragility: side effects don't backtrack

LPEG discards a failed alternative's *captures* but not the *side effects* its
`Cmt` callbacks performed. Every push must therefore be manually paired with a
pop **on every exit path**, which the grammar does with:

```moon
Cut = P -> false                       -- always-fail
ensure = (patt, finally) ->            -- run `finally` on success AND failure
  patt * finally + finally * Cut
```

e.g. `TableLitLine: PushIndent * ((TableValueList * PopIndent) + (PopIndent * Cut)) + Space`,
`With: key"with" * DisableDo * ensure(WithExp, PopDo) * ...`. This idiom is
easy to get wrong (a push inside a partially-matched pattern that fails later
leaks state), it obscures the grammar, and it is a known source of
long-standing parser oddities. **This is the main thing we should fix rather
than replicate.**

### Other machinery worth noting

- **Per-parse state**: `build_grammar!` closes over fresh stacks per parser
  instance; `state.last_pos` is updated in `check_indent` and used to report
  the error line on failure.
- **Keywords**: `key"if"` etc. register into a `keywords` table; `Name` is a
  `Cmt` that rejects registered keywords.
- **AST construction**: heavy use of `/ fn` transformation captures —
  `mark(name)` produces `{name, ...}` nodes, `pos(patt)` stores the position
  in `node[-1]`, plus structural helpers (`format_assign`, `join_chain`,
  `flatten_or_mark`, `wrap_decorator`, `self_assign`) and validation
  (`check_assignable`, which `error({node, msg})`s out of the match, caught by
  the `xpcall` in `file_parser`).
- **Long strings** use `Cg`/`Cb` backreferences (pgen already has `Cmb` for
  this). Comments are consumed by `Space`. `lpeg.setmaxstack 10000` hints at
  deep recursion in real-world sources.

## 2. Where the current pgen prototype falls short

`spec/parsers/moonscript.lua` proves the shape works (CheckIndent / Advance /
PopIndent expressed as `Cmt`), but:

1. **Global mutable state**: the indent stack lives in `_G._ms_indent` and
   must be reset by the caller before each parse. Not reentrant, not
   encapsulated, easy to corrupt on a failed parse.
2. **No backtrack safety**: same problem as LPEG — a failed alternative leaves
   pushes behind. The prototype only works because its toy grammar rarely
   backtracks across a push. A full MoonScript grammar backtracks constantly.
3. **Performance**: every `Cmt` call crosses into Lua and pushes the *entire
   subject string* as an argument (`generate_cmt_code` does
   `lua_pushlstring(parser->L, parser->input, parser->input_len)`), per line,
   per attempted alternative. Indentation checks are the hottest path in an
   indent grammar; they should stay in C.
4. **Error reporting**: no equivalent of `state.last_pos`, so failures give no
   location.

## 3. Proposed engine additions

### 3.1 Indenter objects: configured stack instances (core primitive)

The primitive is an **object constructed with indent configuration** whose
methods produce the indent-related pattern nodes. Each constructed instance
owns one integer stack that sits alongside the parser (a member of the
generated `Parser` struct), (re)initialized at the start of every `parse()`
call — no globals, reentrant by construction.

```lua
local ind = pgen.indenter{
  tab_width = 4,        -- width contributed by "\t" (space = 1)
  initial = 0,          -- seed value for the stack
  -- charset for measurement is fixed to " \t" initially; configurable later
}

return {
  "File",
  Line     = ind.check * V"Statement",          -- measure S" \t"^0 (consuming), succeed if width == top
  InBlock  = ind.advance * V"Block" * ind.pop,  -- non-consuming (lookahead built in);
                                                --   push measured width if > top and top != -1
  TableLitLine = ind.push * V"TableValueList" * ind.pop,  -- unconditional push of measured width
  ClassExtends = ind.prevent * V"Exp" * ind.pop,          -- push sentinel -1 (disables advance)
}
```

Notes:

- **Consumption semantics are baked in** to match how the original grammar
  uses each op: `check` consumes the leading whitespace, `advance` does not
  (the original wraps it in `L()` so the body's `check` re-reads it).
- The ops produce plain pattern nodes carrying a reference to the instance,
  e.g. `{type = types.Ind, op = "advance", indenter = <instance>}`. The
  instance itself is a plain table (config + marker) so the `-j` JSON dump
  keeps working. All ops are zero-capture (like `T`); they participate in
  matching only.
- **Instance discovery mirrors Cmt collection**: at compile time the generator
  traverses the grammar, collects every referenced indenter, and assigns each
  a stack id (same pattern as `cmt_id` assignment, `generator.lua:73`).
  Assigning ids at compile time — not construction time — keeps output
  deterministic when grammars are composed. Codegen can then specialize each
  op per instance's config (e.g. `advance` is one fused C block:
  measure → compare top → push).
- New type in `pgen/types.lua`; leaf-node traversal (preserving the instance
  ref on copy) in `pgen/visitor.lua`; the optimizer must treat these ops as
  side-effecting — never dedupe, reorder, or merge them across choice
  branches (an explicit guard in the trie builder, same spirit as
  `contains_cg_in_pattern`).

### 3.2 The do-stack: same object family, constant ops

MoonScript's second stack (`_do_stack`) is not about indentation, so it
shouldn't be forced through the indent ops. The indenter is a configured
flavor of a general "parser stack" object; every instance also exposes
constant-valued ops:

```lua
local dos = pgen.indenter{ initial = 1 }   -- or a `pgen.stack{}` alias
dos.cpush(0)          -- DisableDo
dos.ctop("ne", 0)     -- check_do: succeed if top != 0
dos.pop()             -- PopDo
```

Same codegen, same stack storage, same trail (3.3) — one mechanism covers
both of the original parser's state stacks.

### 3.3 Transactional semantics: state rolls back with backtracking

**This is the key design decision, and a deliberate improvement over LPEG.**
Stack operations are recorded on an undo trail; when the parser backtracks,
the trail is rewound and every push/pop since the choice point is reversed.

Implementation sketch (all in `pgen/generator.lua`):

- Trail entry: `{uint8 stack_id; uint8 op; int value}` in a growable array on
  the Parser, where `stack_id` is the indenter instance's compile-time id;
  any push op records `(id, PUSH)`, any pop records `(id, POP, popped_value)`.
- `ParserPosition` (`generator.lua:322`) gains `int trail_index`;
  `REMEMBER_POSITION` saves it, `RESTORE_POSITION` rewinds the trail to it
  (pop what was pushed, re-push what was popped).
- Audit every other backtrack site that resets `parser->pos` without the
  macros: the repeat loops' `before_pos` (`generate_repeat_code`), negate,
  lookahead `L`, and `Cmt`'s `cmt_start_pos` restore — each needs to save and
  rewind the trail index too. Since `P`/`R`/`S`/etc. never touch the trail,
  the common fast paths pay nothing (the rewind loop body only runs when
  entries exist).

Consequences for the grammar:

- `ensure()` and `Cut` disappear entirely. `InBlock = ind.advance * Block * ind.pop`
  is correct even when `Block` fails halfway.
- `DisableDo ... PopDo` pairs become a simple sequence with no failure
  choreography.
- Parses that fail leave the parser clean for the next call.

### 3.4 Transformation captures (`Cx`) — recommended but separable

The original grammar builds its AST with `/ fn` captures. Two options:

- **(a) Tag-table convention (works today)**: `Ct(Cc"if" * ...)` produces
  exactly `mark"if"`'s `{"if", ...}` shape, as the prototype already does.
  Structural transforms (`format_assign`, `join_chain`, `flatten_or_mark`,
  `wrap_decorator`, assignability validation) move to a **post-parse
  normalization pass** in plain Lua over the returned tree. `pos()`/`node[-1]`
  is handled by capturing `Cp()` as a positional element and relocating it in
  the same pass. This keeps the C parser dumb and fast, and the pass is
  ordinary testable Lua.
- **(b) Add `Cx(patt, lua_code)`**: like lpeg's `patt / fn`, a capture-time
  transform. ~90% of the implementation is already in `generate_cmt_code` —
  `Cx` is `Cmt` minus subject/pos arguments and minus match-control (always
  succeeds, replaces inner captures with return values). Cheap to add and
  makes in-flight transforms (e.g. `join_chain`, which is awkward to defer)
  natural.

Recommendation: do (a) for the bulk of the port, add (b) early because a
handful of rules genuinely want parse-time shaping, and `Cx` is low-cost given
the existing Cmt infrastructure.

### 3.5 Error reporting

Replace `state.last_pos` with native **furthest-failure tracking** (already on
the TODO): the parser records the maximum `pos` reached before a failure
(cheaply — a compare in the fail path of terminal patterns, or just in
`CheckIndent`-class primitives to start). `parse()` returns
`nil, furthest_pos` on failure, and a Lua-side helper formats the
line/column message like `file_parser` does today. `T()` labels layer on top
of this for targeted messages later.

## 4. Porting strategy for the MoonScript grammar

Structure: `moonscript_pgen/grammar.lua` (the pgen grammar),
`moonscript_pgen/tree.lua` (post-parse normalization + assignability checks).
Port in stages, each with specs:

1. **Literals & lexical layer**: `Space`/`Break`/`Comment`/`Num`/`Shebang`
   from `parse/literals.moon` — direct translation. **Keywords without Cmt**:
   build `Keyword = (P"if" + P"then" + ...) * -AlphaNum` and define
   `Name = ident - Keyword`; pgen's trie optimization turns the keyword choice
   into a fast trie, removing the original's per-name Lua callback.
2. **Strings**: single/double quoted with `#{}` interpolation (recursive
   `V"Exp"`), Lua long strings via existing `Cg`/`Cmb` backreference support
   (this is what `Cmb` was built for; verify `[==[` level matching works —
   original uses a length-compare Cmt, ours matches the captured text
   exactly, which is equivalent here).
3. **Expressions**: `Exp`/`Value`/`Chain`/`Callable`/operators — no indent
   involvement; largest but most mechanical chunk.
4. **Statements & blocks**: `Block`/`Line`/`Body`/`Advance` machinery using
   indenter ops, then `if`/`while`/`for`/`with`/`switch` + the do-stack.
5. **The hard indent customers**: multi-line table literals
   (`PushIndent`/`PopIndent` at arbitrary depth), `TableBlock`, `ClassBlock` +
   `PreventIndent`, open `InvokeArgs`/`ArgBlock`, statement decorators.
6. **Normalization pass + error messages.**

### Testing

- Unit specs per stage under `spec/` as usual.
- **Differential testing** is the big win: the reference implementation is
  sitting in `moonscript/`. A spec that runs both `moonscript.parse.string()`
  and the pgen parser over (a) the original repo's `spec/inputs/*.moon` files
  and (b) MoonScript's own `.moon` sources, asserting identical trees (modulo
  the `node[-1]` position convention). This pins down every corner of the
  grammar without hand-writing thousands of cases.

### Original-parser oddities to re-evaluate (invited deviations)

- **State leaks across failed alternatives** (the `ensure`/`Cut` machinery):
  fixed by transactional stacks; some historical MoonScript parse quirks may
  legitimately disappear. Differential tests will surface any input where
  behavior changes; treat each as a decision, not a bug.
- **Tab = 4 spaces, mixing allowed**: keep as default for compatibility
  (`W` tab_width option), but consider a strict mode that rejects mixed
  indentation on one line.
- **`state.last_pos` error location** (reports last indent check, not deepest
  progress): replaced by furthest-failure, which gives strictly better
  locations.
- **`lpeg.setmaxstack 10000`**: pgen emits recursive C functions; the
  `parser->depth` guard should get a configurable limit + clean error rather
  than stack overflow on pathological nesting.

## 5. Edge cases and accepted compromises

Decisions made up front so implementation doesn't relitigate them:

- **`advance` must be a fused primitive, not sugar.** The original
  `Advance = L(Cmt(Indent, advance_indent))` works only because LPEG does
  *not* roll back Cmt side effects — the push survives the lookahead's
  position reset. Under transactional semantics, generic `L()`/negate rewind
  the trail (predicates are state-pure; both already use
  `REMEMBER/RESTORE_POSITION`), so `L(push)` would undo the push. `ind.advance`
  therefore bakes in "reset position, keep the push." This is the one place
  the original grammar's reliance on non-rollback semantics is load-bearing.
- **Prevent-sentinel**: `advance_indent` needs `top != -1 and width > top`
  because any width is `> -1`. Use `INT_MAX` internally as the prevent
  sentinel so the plain `>` comparison covers it with no special case.
- **Keep user-visible indent quirks verbatim** (they define valid MoonScript):
  tab = 4 with free space/tab mixing; table-literal continuation lines at any
  column (`PushIndent` pushes whatever it measures, no comparison); child
  blocks at any strictly-deeper width while siblings match exactly;
  comment-only lines count as blank (`Space` eats comments). Strict modes are
  future opt-ins.
- **Keep one `Cmt` for `check_assignable` initially.** It is genuinely
  match-time: a non-assignable chain makes `Assignable` fail and falls through
  to the next alternative (`ClassDecl`). Could later become a structural rule
  (chain variant ending in dot/index/slice). If any Cmt stays on a hot path,
  intern the subject string once in the registry instead of pushing the whole
  input per call.
- **Post-parse pass over `Cx` for anything that throws.** LPEG defers `/ fn`
  evaluation to the end of the total match, so the original's transforms
  (including `format_assign`'s "not assignable" error) already run
  effectively post-parse. A Lua normalization pass is semantically *closer*
  to the original than eager `Cx` (which would run mid-parse and throw from
  branches the original backtracks out of). `Cx` is demoted to nice-to-have
  for pure transforms.
- **Don't replicate state-leak bugs.** The original has latent leaks (e.g. in
  `InBlock: Advance * Block * PopIndent`, a push leaks if `Block` fails after
  `Advance` pushed). Transactional rollback silently fixes these, so
  differential mismatches are not automatically our bug — triage each as a
  "who's right?" decision, keeping our behavior unless real-world `.moon`
  code depends on the quirk.
- **Error position quality over message parity**: furthest-failure beats
  `state.last_pos`; don't chase exact message text.
- **Carry-overs the prototype currently misses**: CRLF line breaks
  (`Break = P"\r"^-1 * P"\n"`), do-stack "empty means allowed" initial
  condition, LuaJIT number suffixes in `Num`.

Known risks to watch: (a) eager captures + heavy expression backtracking —
pgen builds captures on the Lua stack during the parse where LPEG defers
them, so benchmark expression-heavy input early (this, not the indent ops, is
the likely perf hotspot); (b) recursion depth on deeply nested sources —
give `parser->depth` a configurable limit and clean error.

## 6. Implementation milestones

1. **[DONE] Engine — stacks + trail** (`generator.lua`, `types.lua`,
   `visitor.lua`): `pgen.indenter` instances with their op nodes
   (`check`/`advance`/`push`/`pop`/`prevent` + constant ops `cpush`/`ctop`),
   compile-time stack id assignment, `ParserPosition.trail_index`, all
   backtrack sites audited (REMEMBER/RESTORE macros + Cmt callback-failure
   path). Specs in `spec/indent_spec.lua` exercise backtracking across
   pushes, lookahead purity, Cmt rollback, prevent, and tab width config.
2. **[DONE] Rewrite `spec/parsers/moonscript.lua`** on top of the indenter
   object (drops `_G._ms_indent`, all existing specs pass unchanged).
3. **[DONE] Engine hardening for real-world grammars** — crash-class issues
   that toy grammars never trigger but an ~80-rule grammar over arbitrary
   input will:
   - **Recursion depth limit**: rule functions track `parser->depth`
     unconditionally and raise a Lua error past `PGEN_MAX_DEPTH` (default
     5000, `max_depth` compile option). A Lua error rather than a match
     failure so predicates/choices can't convert the overflow into a wrong
     parse.
   - **Lua stack headroom**: every capture push goes through
     `pgen_checkstack`; overflowing the Lua stack (LUAI_MAXCSTACK, 8000 in
     stock 5.1) is now a clean Lua error instead of undefined behavior.
     Remaining ceiling: a single Ct holding >8000 pending captures (e.g. one
     block with that many statements) hits this limit — future work could
     fold captures into the table incrementally.
   - **Zero-width repeat guard**: `pgen.compile` now rejects unbounded
     repetitions with nullable bodies at compile time (`pgen/analyze.lua`),
     matching LPeg's "loop body may accept empty string" error. Conservative
     for recursive rule references.
4. **[DONE] Furthest-failure tracking.** The engine records the deepest
   failed match attempt (multi-char literals, tries, predicates, Cmb/Cmt,
   indenter ops; single-char matchers skipped as noise) and ordinary
   failures return `nil, message|nil, furthest_pos`. A/B benchmarks (JSON +
   full MoonScript corpus): overhead indistinguishable from code-layout
   noise (≤1% worst case, verified against a no-op-macro build);
   `-DPGEN_NO_FURTHEST` removes it. `moonscript_pgen` errors now format as
   line/column messages via `pgen.errors`. `Cx` was never needed. Fixed
   along the way: generated Lua 5.1 `luaopen_` used `luaL_register` with a
   global name, so two same-named parsers in one process silently shared
   (and clobbered) one module table.

   **`T()` labels: attempted and removed.** Candidate commit points that
   looked safe by construction (unterminated strings at EOF, malformed
   `import` after its keyword) were refuted by the truncation fuzz: `Value`
   speculatively parses arbitrary text as code — at `[[` the Comprehension
   alternative consumes both brackets and probes long-string *content* as an
   expression before `String` is attempted — so labels fire mid-speculation
   and reject valid programs (`y = [[if x then import a]] .. z`,
   `x = [[=[hi]] .. 1`, and real Lapis template files). Regression cases
   live in the diff spec. Conclusion: in this grammar labels need an
   engine-level catch/recovery operator (lpeglabel's `Rec`) that scopes a
   label back to an ordinary failure; until then, furthest-failure alone
   carries error reporting.
5. **[DONE] Grammar port** (all stages) — `moonscript_pgen/grammar.lua`, a
   full transcription of the reference grammar. The `/ fn` transformation
   captures become `Ct(Cc"tag" * ...)` nodes; the nontrivial transforms
   (`pos`, `flatten_or_mark`, `format_assign`, `join_chain`,
   `wrap_decorator`, `self_assign`, lua-string delimiters) are emitted with
   `@`-prefixed tags. Long strings use `Cg`/`Cmb` on the `=` run;
   `check_assignable` stays a `Cmt` (per §5); the do-stack is a second
   indenter with constant ops (`initial = 1`, `cpush(0)`/`ctop("ne", 0)`);
   all `ensure`/`Cut` cleanup dropped in favor of transactional rollback.
   `Cx` turned out to be unnecessary.
6. **[DONE] Normalization pass** — `moonscript_pgen/tree.lua` resolves the
   `@` tags bottom-up (matching LPeg's deferred innermost-first evaluation),
   including `error({node, msg})` for non-assignable targets, exactly like
   the reference. `moonscript_pgen/init.lua` wraps compile + parse +
   normalize behind `parse.string()`.
7. **[DONE] Differential test harness** — `spec/moonscript_diff_spec.lua`
   compares trees (including `[-1]` positions) against the reference parser
   over every `.moon` file in `moonscript/` (80 files, 175KB) plus targeted
   snippets; `spec/moonscript_pgen_spec.lua` holds standalone hand-checked
   anchors. Result: **zero mismatches**, including an 8.5k-case
   line-boundary truncation fuzz (agreement on success/failure and trees).
   No reference-parser quirk deviations surfaced — transactional rollback
   produced identical trees on the entire corpus. Corpus parse timing:
   ~1.6x faster than the reference API (which rebuilds its grammar per
   call), ~0.7x of a reused LPeg grammar; the Lua-side normalize pass is a
   third of our total, and eager capture construction remains the known
   optimization target.

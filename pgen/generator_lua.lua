-- Lua code generation target: compiles a grammar into a self-contained pure
-- Lua module (5.1+, LuaJIT) with the same parse() contract as the C target,
-- for environments without a C compiler.
--
-- The generated parser mirrors the C target's structure: a recursive-descent
-- matcher over a parser state table, with captures recorded in a log during
-- matching and materialized only after the parse succeeds. Positions are
-- 0-based internally (like the C parser) and converted at the API boundary.
local generator = {}
local types = require("pgen.types")
local common = require("pgen.codegen_common")

local template_code = common.template_code
local sorted_rules = common.sorted_rules

-- Pattern tables overload __len for lookahead on Lua 5.2+, so use the raw
-- array length when inspecting their children. Lua 5.1 ignores __len on
-- tables, making # equivalent to rawlen there.
local raw_length = rawlen or function(value)
  return #value
end

local char_escapes = {
  [7] = "\\a", [8] = "\\b", [9] = "\\t", [10] = "\\n", [11] = "\\v",
  [12] = "\\f", [13] = "\\r", [34] = "\\\"", [92] = "\\\\"
}

-- Escaped body of a Lua string literal (no surrounding quotes). Non-printable
-- and non-ASCII bytes use three-digit decimal escapes so the output is
-- deterministic regardless of the Lua version running the generator.
local function escape_text(str)
  local out = str:gsub(".", function(c)
    local b = string.byte(c)
    if char_escapes[b] then
      return char_escapes[b]
    end
    if b >= 32 and b <= 126 then
      return c
    end
    return string.format("\\%03d", b)
  end)
  return out
end

local function lua_string_literal(str)
  return '"' .. escape_text(str) .. '"'
end

local function lua_number_literal(v)
  if v ~= v then return "(0/0)" end
  if v == math.huge then return "(1/0)" end
  if v == -math.huge then return "(-1/0)" end
  if v % 1 == 0 and v >= -2^53 and v <= 2^53 then
    return string.format("%d", v)
  end
  return string.format("%.17g", v)
end

local function lua_value_literal(v)
  local t = type(v)
  if t == "string" then
    return lua_string_literal(v)
  elseif t == "number" then
    return lua_number_literal(v)
  elseif t == "boolean" then
    return tostring(v)
  end
  error("Unsupported constant capture type: " .. t)
end

-- Error message assignment statement, or "" when error messages are
-- disabled (the pgen_errors option, PGEN_ERRORS in the C target). expr is a
-- Lua expression string.
local function err_stmt(context, expr)
  if not context.errors then
    return ""
  end
  return "parser.error_message = " .. expr
end

-- Snapshot/restore statements for a backtrack point. Patterns that cannot
-- change rollback state (captures, indenter ops) only save the input
-- position; nested do-blocks shadow the snapshot locals, matching the C
-- macros' block scoping.
local function position_ops(pattern, context)
  local needs_state = context.analyze.changes_backtrack_state(
    pattern, context.rules, context.stateful_rules)
  if needs_state then
    if context.has_indenters then
      return "local pp_pos, pp_cap, pp_trail = parser.pos, parser.cap_n, parser.trail_n",
        "parser.pos = pp_pos parser.cap_n = pp_cap ind_trail_rewind(parser, pp_trail)"
    end
    return "local pp_pos, pp_cap = parser.pos, parser.cap_n",
      "parser.pos = pp_pos parser.cap_n = pp_cap"
  end
  return "local pp_pos = parser.pos", "parser.pos = pp_pos"
end

-- Collect the alternatives of a (possibly nested) binary choice tree into a
-- flat list, so long choices emit as a flat chain of guarded alternatives
-- instead of nesting one syntax level per alternative (Lua caps nesting
-- depth around 200 levels).
local function flatten_choice(node, out)
  for i = 1, raw_length(node) do
    local child = node[i]
    if type(child) == "table" and child.type == "choice" then
      flatten_choice(child, out)
    else
      out[#out + 1] = child
    end
  end
  return out
end

-- Intern a character set, returning its index in the generated sets table
local function set_index(context, set)
  local idx = context.set_index[set]
  if not idx then
    idx = #context.set_list + 1
    context.set_index[set] = idx
    context.set_list[idx] = set
  end
  return idx
end

-- Generate code for a pattern
function generator.generate_pattern_code(pattern, context)
  local t = pattern.type

  if t == types.P then
    local literal = pattern.value
    if type(literal) == "number" then
      return generator.generate_n_chars_code(literal, context)
    else
      return generator.generate_literal_code(literal, context)
    end
  elseif t == types.R then
    return generator.generate_range_code(pattern.value, context)
  elseif t == types.S then
    return generator.generate_set_code(pattern.value, context)
  elseif t == types.V then
    return template_code([[rules[$RULE$](parser)]], {
      RULE = lua_string_literal(tostring(pattern.value))
    })
  elseif t == types.C then
    return generator.generate_capture_code(pattern.value, context)
  elseif t == types.Ct then
    return generator.generate_capture_table_code(pattern.value, context)
  elseif t == types.Cp then
    return "cap_push(parser, CAP_POS, nil, parser.pos, 0) -- position capture"
  elseif t == types.Cc then
    return generator.generate_constant_capture_code(pattern.value, context)
  elseif t == types.L then
    return generator.generate_lookahead_code(pattern.value, context)
  elseif t == types.Cg then
    return generator.generate_capture_group_code(pattern.value, pattern.name, context)
  elseif t == types.Cn then
    return generator.generate_numbered_capture_code(pattern.value, pattern.name, context)
  elseif t == types.Cmb then
    return generator.generate_capture_match_back_code(pattern.name, context)
  elseif t == types.Cmt then
    return generator.generate_cmt_code(pattern.value, pattern.cmt_id, context)
  elseif t == types.Cfn then
    return generator.generate_cfn_code(pattern.value, pattern.cmt_id, context)
  elseif t == types.T then
    return generator.generate_labeled_failure_code(pattern.value, context)
  elseif t == types.Ind then
    return generator.generate_indenter_code(pattern, context)
  elseif t == "sequence" then
    return generator.generate_sequence_code(pattern, context)
  elseif t == "choice" then
    return generator.generate_choice_code(pattern, context)
  elseif t == "dispatch_choice" then
    return generator.generate_dispatch_choice_code(pattern, context)
  elseif t == "repeat" then
    return generator.generate_repeat_code(pattern[1], pattern[2], context)
  elseif t == "negate" then
    return generator.generate_negate_code(pattern[1], context)
  elseif t == "literal_trie" then
    return generator.generate_trie_code(pattern.trie, pattern.strings)
  else
    error("Unknown pattern type: " .. tostring(t))
  end
end

function generator.generate_literal_code(literal, context)
  if #literal == 1 then
    return template_code([[-- match single character $DISPLAY$
if byte(parser.input, parser.pos + 1) == $CHAR_CODE$ then
  parser.pos = parser.pos + 1
else
  parser.success = false
  $ERR$
end]], {
      DISPLAY = lua_string_literal(literal),
      CHAR_CODE = string.byte(literal),
      ERR = err_stmt(context, lua_string_literal(
        "Expected character `" .. escape_text(literal) .. "` at position ") .. " .. parser.pos")
    })
  end

  return template_code([[-- match literal $DISPLAY$
if sub(parser.input, parser.pos + 1, parser.pos + $LEN$) == $LITERAL$ then
  parser.pos = parser.pos + $LEN$
else
  parser.success = false
  record_furthest(parser)
  $ERR$
end]], {
    DISPLAY = lua_string_literal(literal),
    LITERAL = lua_string_literal(literal),
    LEN = #literal,
    ERR = err_stmt(context, lua_string_literal(
      "Expected `" .. escape_text(literal) .. "` at position ") .. " .. parser.pos")
  })
end

function generator.generate_n_chars_code(n, context)
  return template_code([[-- match any $N$ characters
if parser.pos + $N$ <= parser.input_len then
  parser.pos = parser.pos + $N$
else
  parser.success = false
  record_furthest(parser)
  $ERR$
end]], {
    N = n,
    ERR = err_stmt(context, lua_string_literal(
      "Expected at least " .. n .. " more characters at position ") .. " .. parser.pos")
  })
end

-- ranges: array of two-character strings representing low and upper bounds
function generator.generate_range_code(ranges, context)
  local conditions = {}
  local display = {}

  for _, range in ipairs(ranges) do
    local range_left, range_right = range:match("^(.)(.)")
    assert(range_left, "range must have two characters: " .. range)
    assert(range_left <= range_right, "range must be in ascending order: " .. range)

    table.insert(conditions, template_code("(rb >= $LOW$ and rb <= $HIGH$)", {
      LOW = string.byte(range_left),
      HIGH = string.byte(range_right)
    }))
    table.insert(display, escape_text(range_left) .. " - " .. escape_text(range_right))
  end

  return template_code([[do -- match character range $DISPLAY$
  local rb = byte(parser.input, parser.pos + 1)
  if rb and ($CONDITION$) then
    parser.pos = parser.pos + 1
  else
    parser.success = false
    $ERR$
  end
end]], {
    DISPLAY = lua_string_literal(table.concat(ranges, ",")),
    CONDITION = table.concat(conditions, " or "),
    ERR = err_stmt(context, lua_string_literal(
      "Expected character in ranges [" .. table.concat(display, ", ") ..
      "] at position ") .. " .. parser.pos")
  })
end

function generator.generate_set_code(set, context)
  local idx = set_index(context, set)

  -- The set literal appears quoted inside the message, matching the C target
  local msg = ""
  if context.errors then
    local prefix = lua_string_literal(
      "Expected one of \"" .. escape_text(set) .. "\" at position ")
    msg = template_code([[

    if sb then
      parser.error_message = $PREFIX$ .. parser.pos
    else
      parser.error_message = $PREFIX$ .. parser.pos .. " but reached end of input"
    end]], {PREFIX = prefix})
  end

  return template_code([[do -- match character set $DISPLAY$
  local sb = byte(parser.input, parser.pos + 1)
  if sb and sets[$IDX$][sb] then
    parser.pos = parser.pos + 1
  else
    parser.success = false$MSG$
  end
end]], {
    DISPLAY = lua_string_literal(set),
    IDX = idx,
    MSG = msg
  })
end

function generator.generate_sequence_code(patterns, context)
  if raw_length(patterns) == 1 then
    -- TODO: throw an error here? shouldn't happen
    return generator.generate_pattern_code(patterns[1], context)
  end

  local remember, restore = position_ops(patterns, context)

  -- Emitted flat (each step guarded separately) rather than nested like the
  -- C target: failed patterns leave no state behind, so restoring after a
  -- first-step failure is a no-op and the flat form is equivalent
  local steps = {}
  for i = 1, raw_length(patterns) do
    local body = generator.generate_pattern_code(patterns[i], context)
    if i == 1 then
      steps[i] = body
    else
      steps[i] = "if parser.success then\n" .. body .. "\nend"
    end
  end

  return template_code([[do -- sequence with $N$ patterns
  $REMEMBER$
  $STEPS$
  if not parser.success then
    $RESTORE$
  end
end]], {
    N = raw_length(patterns),
    REMEMBER = remember,
    STEPS = table.concat(steps, "\n"),
    RESTORE = restore
  })
end

-- Shared ordered-choice protocol: an alternative may only be tried after an
-- ordinary failure, never after a labeled failure from T(). Choice and
-- dispatch_choice must both emit exactly this protocol.
local function generate_alternative_code(body, extra_condition, preamble)
  return template_code([[if not parser.success and not parser.throw_label$EXTRA$ then
  $PREAMBLE$parser.success = true
  $BODY$
end]], {
    EXTRA = extra_condition and (" and " .. extra_condition) or "",
    PREAMBLE = preamble and (preamble .. "\n  ") or "",
    BODY = body
  })
end

function generator.generate_choice_code(pattern, context)
  local alternatives = flatten_choice(pattern, {})
  local chunks = {}
  for i, alternative in ipairs(alternatives) do
    local body = generator.generate_pattern_code(alternative, context)
    if i == 1 then
      chunks[i] = body
    else
      chunks[i] = generate_alternative_code(body)
    end
  end

  return template_code([[do -- choice with $N$ alternatives
  $ALTERNATIVES$
end]], {
    N = #alternatives,
    ALTERNATIVES = table.concat(chunks, "\n")
  })
end

-- Generate a non-consuming FIRST-byte dispatcher. The dispatch tables are
-- built once at module load: a byte -> mask table where each interned mask
-- records which alternatives may match ([i] = true), which attempted
-- alternatives had a lower alternative skipped (s[i] = true, for furthest
-- failure parity), and whether no alternative was skipped at all (full).
function generator.generate_dispatch_choice_code(pattern, context)
  local n = raw_length(pattern)
  local disp_id = #context.dispatch_inits + 1

  local mask_defs = {}   -- candidate key -> {name = local name}
  local mask_order = {}  -- deterministic emission order

  local function intern_mask(candidates)
    local key = table.concat(candidates, ",")
    local def = mask_defs[key]
    if not def then
      local has = {}
      for _, idx in ipairs(candidates) do has[idx] = true end

      local members = {}
      for _, idx in ipairs(candidates) do
        members[#members + 1] = "[" .. idx .. "] = true"
      end

      local skips = {}
      local missing_below = false
      for i = 1, n do
        if has[i] and missing_below then
          skips[#skips + 1] = "[" .. i .. "] = true"
        end
        if not has[i] then
          missing_below = true
        end
      end

      def = {
        name = "dmask" .. (#mask_order + 1),
        members = table.concat(members, ", "),
        skips = table.concat(skips, ", "),
        full = #candidates == n
      }
      mask_defs[key] = def
      mask_order[#mask_order + 1] = def
    end
    return def.name
  end

  local byte_masks = {}
  local counts = {}
  for b = 0, 255 do
    local name = intern_mask(pattern.byte_candidates[b])
    byte_masks[b] = name
    counts[name] = (counts[name] or 0) + 1
  end
  local eof_name = intern_mask(pattern.eof_candidates)

  -- The most common mask becomes the loop-filled default
  local default_name, default_count = nil, -1
  for _, def in ipairs(mask_order) do
    local count = counts[def.name] or 0
    if count > default_count then
      default_name, default_count = def.name, count
    end
  end

  local def_lines = {}
  for _, def in ipairs(mask_order) do
    def_lines[#def_lines + 1] = template_code(
      "local $NAME$ = { $MEMBERS$ s = { $SKIPS$ }, full = $FULL$ }", {
        NAME = def.name,
        MEMBERS = def.members == "" and "" or (def.members .. ","),
        SKIPS = def.skips,
        FULL = tostring(def.full)
      })
  end

  local overrides = {}
  for b = 0, 255 do
    if byte_masks[b] ~= default_name then
      overrides[#overrides + 1] = template_code("bytes[$B$] = $NAME$", {
        B = b, NAME = byte_masks[b]
      })
    end
  end

  context.dispatch_inits[disp_id] = template_code([[do -- dispatch tables for choice $ID$
  $DEFS$
  local bytes = {}
  for b = 0, 255 do bytes[b] = $DEFAULT$ end
  $OVERRIDES$
  disp[$ID$] = { bytes = bytes, eof = $EOF$ }
end]], {
    ID = disp_id,
    DEFS = table.concat(def_lines, "\n  "),
    DEFAULT = default_name,
    OVERRIDES = table.concat(overrides, "\n  "),
    EOF = eof_name
  })

  local alternatives = {}
  local replays = {}
  for i = 1, n do
    local body = generator.generate_pattern_code(pattern[i], context)

    -- Any skipped alternative before this candidate would have failed at the
    -- current position; record it so the furthest failure position matches
    -- the undispatched choice.
    local preamble
    if i > 1 then
      preamble = "if dm.s[" .. i .. "] then record_furthest(parser) end"
    end

    alternatives[#alternatives + 1] = generate_alternative_code(
      body, "dm[" .. i .. "]", preamble)

    if context.errors then
      -- Replays run the whole chain in original order so error_message ends
      -- up written by the same alternative as in the undispatched choice.
      -- Every alternative is known to fail here.
      replays[#replays + 1] = generate_alternative_code(body)
    end
  end

  local replay_code = ""
  if context.errors then
    replay_code = template_code([[

    if not dm.full then
      $REPLAYS$
    end]], {REPLAYS = table.concat(replays, "\n")})
  end

  return template_code([[do -- FIRST-byte dispatched ordered choice
  local dd = disp[$ID$]
  local db = byte(parser.input, parser.pos + 1)
  local dm = db and dd.bytes[db] or dd.eof
  parser.success = false
  $ALTERNATIVES$
  if not parser.success and not parser.throw_label then
    record_furthest(parser)$REPLAY$
  end
end]], {
    ID = disp_id,
    ALTERNATIVES = table.concat(alternatives, "\n"),
    REPLAY = replay_code
  })
end

-- if n is zero or positive, at least n repetitions
-- if n is negative, at most n repetitions
function generator.generate_repeat_code(a, n, context)
  if n < 0 then
    return template_code([[do -- at most $N$ repetitions
  local rep_count = 0
  while rep_count < $N$ do
    local before_pos = parser.pos
    $BODY$
    if not parser.success or before_pos == parser.pos then
      -- Break on failure or zero-width match
      -- Only recover from ordinary failure, not labeled failure from T()
      if not parser.throw_label then
        parser.success = true
      end
      break
    end
    rep_count = rep_count + 1
  end
end]], {
      N = -n,
      BODY = generator.generate_pattern_code(a, context)
    })
  end

  if n == 0 then
    return template_code([[do -- zero or more repetitions
  while true do
    $BODY$
    if not parser.success then
      break
    end
  end
  -- Only recover from ordinary failure, not labeled failure from T()
  if not parser.throw_label then
    parser.success = true
  end
end]], {
      BODY = generator.generate_pattern_code(a, context)
    })
  end

  local remember, restore = position_ops(a, context)

  return template_code([[do -- at least $N$ repetitions
  $REMEMBER$
  local rep_count = 0
  while true do
    $BODY$
    if not parser.success then
      break
    end
    rep_count = rep_count + 1
  end
  if parser.throw_label then
    -- Keep failure state, propagate labeled failure
  elseif rep_count >= $N$ then
    parser.success = true
  else
    $RESTORE$
    $ERR$
  end
end]], {
    N = n,
    REMEMBER = remember,
    RESTORE = restore,
    BODY = generator.generate_pattern_code(a, context),
    ERR = err_stmt(context, lua_string_literal(
      "Expected " .. n .. " repetitions at position ") .. " .. parser.pos")
  })
end

function generator.generate_negate_code(a, context)
  local remember, restore = position_ops(a, context)

  return template_code([[do -- negate (only match if pattern fails)
  $REMEMBER$
  $BODY$
  if parser.success then
    -- Pattern matched, so negate fails
    $RESTORE$
    parser.success = false
    record_furthest(parser)
    $ERR$
  else
    -- Pattern failed, so negate succeeds
    parser.success = true
    -- Swallow labeled failures inside predicates (LPegLabel behavior)
    if parser.throw_label then
      parser.throw_label = nil
      parser.throw_pos = 0
    end
    $RESTORE$
  end
end]], {
    REMEMBER = remember,
    RESTORE = restore,
    BODY = generator.generate_pattern_code(a, context),
    ERR = err_stmt(context,
      '"Negated pattern unexpectedly matched at position " .. parser.pos')
  })
end

function generator.generate_lookahead_code(body, context)
  local remember, restore = position_ops(body, context)

  return template_code([[do -- lookahead (match without consuming input)
  $REMEMBER$
  $BODY$
  if parser.success then
    $RESTORE$
  end
end]], {
    REMEMBER = remember,
    RESTORE = restore,
    BODY = generator.generate_pattern_code(body, context)
  })
end

function generator.generate_capture_code(body, context)
  return template_code([[do -- capture
  local cap_start_pos = parser.pos
  $BODY$
  if parser.success then
    cap_push(parser, CAP_STR, nil, cap_start_pos, parser.pos - cap_start_pos)
  end
end]], {
    BODY = generator.generate_pattern_code(body, context)
  })
end

function generator.generate_capture_table_code(body, context)
  return template_code([[do -- capture table
  local ct_cap_start = parser.cap_n
  cap_push(parser, CAP_TBL_OPEN, nil, 0, 0)
  $BODY$
  if parser.success then
    cap_push(parser, CAP_TBL_CLOSE, nil, 0, 0)
  else
    parser.cap_n = ct_cap_start
  end
end]], {
    BODY = generator.generate_pattern_code(body, context)
  })
end

-- Each value becomes one log entry carrying the constant directly; matching
-- never constructs new values
function generator.generate_constant_capture_code(values, context)
  local push_code = {}
  for i = 1, values.count do
    local value = values[i]
    if value == nil then
      push_code[#push_code + 1] = "  cap_push(parser, CAP_NIL, nil, 0, 0)"
    else
      push_code[#push_code + 1] = template_code(
        "  cap_push(parser, CAP_CONST, $VALUE$, 0, 0)",
        {VALUE = lua_value_literal(value)})
    end
  end

  return template_code([[do -- constant capture ($N$ values)
$PUSH_CODE$
end]], {
    N = values.count,
    PUSH_CODE = table.concat(push_code, "\n")
  })
end

function generator.generate_capture_group_code(body, name, context)
  local name_literal = lua_string_literal(name)
  return template_code([[do -- capture group $NAME$
  local cg_cap_start = parser.cap_n
  cap_push(parser, CAP_GROUP_OPEN, $NAME$, parser.pos, 0)
  $BODY$
  if parser.success then
    cap_push(parser, CAP_GROUP_CLOSE, $NAME$, parser.pos, 0)
  else
    parser.cap_n = cg_cap_start
  end
end]], {
    BODY = generator.generate_pattern_code(body, context),
    NAME = name_literal
  })
end

function generator.generate_numbered_capture_code(body, n, context)
  if n == 0 then
    return template_code([[do -- numbered capture (discard all)
  local cn_cap_start = parser.cap_n
  $BODY$
  if parser.success then
    parser.cap_n = cn_cap_start
  end
end]], {
      BODY = generator.generate_pattern_code(body, context)
    })
  end

  context.features.cap_select = true
  return template_code([[do -- numbered capture (select $N$)
  local cn_cap_start = parser.cap_n
  $BODY$
  if parser.success then
    cap_select(parser, cn_cap_start, $N$)
  end
end]], {
    BODY = generator.generate_pattern_code(body, context),
    N = n
  })
end

function generator.generate_capture_match_back_code(name, context)
  context.features.cap_match_back = true
  return template_code([[-- capture match back $NAME$
parser.success = cap_match_back(parser, $NAME$)
if not parser.success then
  record_furthest(parser)
  $ERR$
end]], {
    NAME = lua_string_literal(name),
    ERR = err_stmt(context, lua_string_literal(
      "Capture match back '" .. name .. "' failed at position ") .. " .. parser.pos")
  })
end

function generator.generate_cmt_code(inner_pattern, cmt_id, context)
  context.features.run_cmt = true

  local trail_snap, trail_rewind = "", ""
  if context.has_indenters then
    trail_snap = "local cmt_trail = parser.trail_n"
    -- Callback rejected the match: undo indenter operations performed by the
    -- inner pattern (an inner failure rewinds itself)
    trail_rewind = [[

    if not parser.success then
      ind_trail_rewind(parser, cmt_trail)
    end]]
  end

  return template_code([[do -- match-time capture (Cmt id=$ID$)
  local cmt_cap_base = parser.cap_n
  local cmt_start_pos = parser.pos
  $TRAIL_SNAP$
  $INNER$
  if parser.success then
    run_cmt(parser, cmt_fns[$ID$], cmt_start_pos, cmt_cap_base)$TRAIL_REWIND$
  end
end]], {
    ID = cmt_id,
    TRAIL_SNAP = trail_snap,
    TRAIL_REWIND = trail_rewind,
    INNER = generator.generate_pattern_code(inner_pattern, context)
  })
end

-- Emits open/close brackets in the capture log carrying the callback and
-- the matched span; the callback runs during materialization, so
-- backtracked-over transforms are never called
function generator.generate_cfn_code(body, cmt_id, context)
  return template_code([[do -- transform capture (Cfn id=$ID$)
  local fn_cap_start = parser.cap_n
  cap_push(parser, CAP_FN_OPEN, cmt_fns[$ID$], parser.pos, 0)
  $BODY$
  if parser.success then
    cap_push(parser, CAP_FN_CLOSE, nil, parser.pos, 0)
  else
    parser.cap_n = fn_cap_start
  end
end]], {
    ID = cmt_id,
    BODY = generator.generate_pattern_code(body, context)
  })
end

function generator.generate_labeled_failure_code(label, context)
  local label_literal = lua_string_literal(label)
  return template_code([[-- throw labeled failure: $LABEL$
parser.success = false
parser.throw_label = $LABEL$
parser.throw_pos = parser.pos
$ERR$]], {
    LABEL = label_literal,
    ERR = err_stmt(context,
      label_literal .. ' .. " at position " .. (parser.pos + 1)')
  })
end

-- All indenter operations are transactional: pushes/pops are recorded on
-- the trail and undone when the parser backtracks past them
function generator.generate_indenter_code(pattern, context)
  local op = pattern.op
  local sid = pattern.stack_id

  if sid == nil then
    error("Ind node has no stack_id assigned; compile the grammar through pgen.compile/generator.generate")
  end

  local vars = {
    -- stack ids are 0-based like the C target; the Lua stacks array is
    -- 1-based, so code indexes with SIDX while messages report SID
    SID = sid,
    SIDX = sid + 1,
    TW = pattern.indenter.tab_width or 4
  }

  if op == "check" then
    vars.ERR = context.errors and
      [[parser.error_message = "Indent width " .. ind_width .. " does not match current level at position " .. parser.pos]] or ""
    return template_code([[do -- indenter check (stack $SID$): consume whitespace, width must equal top
  local ind_width, ind_end = ind_measure(parser, $TW$)
  local ind_s = parser.ind_stacks[$SIDX$]
  if ind_s.n > 0 and ind_s[ind_s.n] == ind_width then
    parser.pos = ind_end
  else
    parser.success = false
    record_furthest(parser)
    $ERR$
  end
end]], vars)
  elseif op == "advance" then
    vars.ERR = context.errors and
      [[parser.error_message = "Indent width " .. ind_width .. " does not advance current level at position " .. parser.pos]] or ""
    return template_code([[do -- indenter advance (stack $SID$): push width if deeper than top, consume nothing
  local ind_width = ind_measure(parser, $TW$)
  local ind_s = parser.ind_stacks[$SIDX$]
  if ind_s.n > 0 and ind_width > ind_s[ind_s.n] then
    ind_push(parser, $SIDX$, ind_width)
  else
    parser.success = false
    record_furthest(parser)
    $ERR$
  end
end]], vars)
  elseif op == "push" then
    return template_code([[do -- indenter push (stack $SID$): consume whitespace, push measured width
  local ind_width, ind_end = ind_measure(parser, $TW$)
  ind_push(parser, $SIDX$, ind_width)
  parser.pos = ind_end
end]], vars)
  elseif op == "prevent" then
    return template_code([[-- indenter prevent (stack $SID$): push sentinel so nested advance fails
ind_push(parser, $SIDX$, IND_PREVENT)]], vars)
  elseif op == "pop" then
    vars.ERR = err_stmt(context, lua_string_literal(
      "Indenter stack " .. sid .. " is empty at position ") .. " .. parser.pos")
    return template_code([[-- indenter pop (stack $SID$)
if not ind_pop(parser, $SIDX$) then
  parser.success = false
  record_furthest(parser)
  $ERR$
end]], vars)
  elseif op == "cpush" then
    vars.VALUE = pattern.value
    return template_code([[-- indenter cpush (stack $SID$): push constant $VALUE$
ind_push(parser, $SIDX$, $VALUE$)]], vars)
  elseif op == "ctop" then
    local cmp_ops = {
      eq = "==", ne = "~=", lt = "<", le = "<=", gt = ">", ge = ">="
    }
    vars.VALUE = pattern.value
    vars.CMP = pattern.cmp
    vars.CMP_OP = cmp_ops[pattern.cmp] or error("Unknown ctop comparison: " .. tostring(pattern.cmp))
    vars.ERR = err_stmt(context, lua_string_literal(
      "Indenter stack " .. sid .. " top failed " .. pattern.cmp .. " " ..
      pattern.value .. " check at position ") .. " .. parser.pos")
    return template_code([[do -- indenter ctop (stack $SID$): top $CMP$ $VALUE$
  local ind_s = parser.ind_stacks[$SIDX$]
  if not (ind_s.n > 0 and ind_s[ind_s.n] $CMP_OP$ $VALUE$) then
    parser.success = false
    record_furthest(parser)
    $ERR$
  end
end]], vars)
  else
    error("Unknown indenter operation: " .. tostring(op))
  end
end

function generator.generate_trie_code(trie, strings)
  local code = generator.generate_trie_node_code(trie)
  local display_strings = {}
  for i, str in ipairs(strings) do
    display_strings[i] = lua_string_literal(str)
  end

  -- Tries are pure literal matchers, so only the input position needs
  -- restoring on failure
  return template_code([[do -- trie match for: $STRINGS$
  local trie_pos = parser.pos
  local last_terminal_pos = 0
  local has_terminal = false
  $TRIE_CODE$
  if not parser.success then
    parser.pos = trie_pos
  end
end]], {
    STRINGS = table.concat(display_strings, ", "),
    TRIE_CODE = code
  })
end

function generator.generate_trie_node_code(node)
  local preamble = ""
  if node.is_terminal then
    preamble = [[if not has_terminal or parser.pos > last_terminal_pos then
  last_terminal_pos = parser.pos
  has_terminal = true
end
]]
  end

  -- Collect and sort cases for deterministic output
  local chars = {}
  for char in pairs(node.children) do
    table.insert(chars, char)
  end
  table.sort(chars)

  local branches = {}
  for i, char in ipairs(chars) do
    local child = node.children[char]
    local case_body

    if child.is_terminal and not next(child.children) then
      -- Leaf node: just advance and succeed
      case_body = "parser.pos = parser.pos + 1"
    elseif child.is_terminal then
      -- Terminal with more children: try to continue, but partial match is OK
      case_body = template_code([[parser.pos = parser.pos + 1
$CHILD_CODE$
if not parser.success then
  -- partial match is valid: $WORD$
  parser.success = true
end]], {
        CHILD_CODE = generator.generate_trie_node_code(child),
        WORD = lua_string_literal(child.word)
      })
    else
      -- Non-terminal: must continue matching
      case_body = "parser.pos = parser.pos + 1\n" ..
        generator.generate_trie_node_code(child)
    end

    branches[#branches + 1] = template_code([[$KEYWORD$ tb == $BYTE$ then -- $CHAR$
  $BODY$]], {
      KEYWORD = i == 1 and "if" or "elseif",
      BYTE = string.byte(char),
      CHAR = lua_string_literal(char),
      BODY = case_body
    })
  end

  -- byte() returns nil at end of input, matching no branch, so the else arm
  -- covers both an unexpected byte and end of input
  return template_code([[$PREAMBLE$local tb = byte(parser.input, parser.pos + 1)
$BRANCHES$
else
  parser.success = false
  if has_terminal then
    parser.pos = last_terminal_pos
    parser.success = true
  else
    record_furthest(parser)
  end
end]], {
    PREAMBLE = preamble,
    BRANCHES = table.concat(branches, "\n")
  })
end

-- Generate a function for a specific rule
function generator.generate_rule_function(name, pattern, context)
  local memo_check, memo_store, start_decl = "", "", ""
  local memo_id = context.memo_ids[name]
  if memo_id then
    start_decl = "local start = parser.pos\n  "
    memo_check = template_code([[
  -- Position-pure rule: a single-slot memo short-circuits the repeated
  -- calls that backtracking alternatives make at the same position
  if parser.memo_pos[$ID$] == start + 1 then
    local memo_end = parser.memo_end[$ID$]
    if memo_end == -1 then
      parser.success = false
      return false
    end
    parser.pos = memo_end
    parser.success = true
    return true
  end
]], {ID = memo_id})
    memo_store = template_code([[
  parser.memo_pos[$ID$] = start + 1
  parser.memo_end[$ID$] = parser.success and parser.pos or -1
]], {ID = memo_id})
  end

  return template_code([[rules[$NAME$] = function(parser)
  $START_DECL$$MEMO_CHECK$local depth = parser.depth + 1
  parser.depth = depth
  if depth > MAX_DEPTH then
    -- A hard Lua error (rather than a match failure) so the overflow can't
    -- be silently converted into a successful parse by a predicate or choice
    error("pgen: max recursion depth (" .. MAX_DEPTH .. ") exceeded at position " .. (parser.pos + 1))
  end

  $BODY$
$MEMO_STORE$
  parser.depth = depth - 1
  return parser.success
end

]], {
    NAME = lua_string_literal(tostring(name)),
    START_DECL = start_decl,
    MEMO_CHECK = memo_check,
    MEMO_STORE = memo_store,
    BODY = generator.generate_pattern_code(pattern, context)
  })
end

-- --- Generated module runtime ---

local CORE_HELPERS = [==[
-- Records the furthest input position where a match attempt failed (only
-- ever increases); parse() reports it when the overall parse fails without
-- a label. Not recorded in single-character matchers, mirroring the C
-- target.
local function record_furthest(parser)
  if parser.pos > parser.furthest_fail then
    parser.furthest_fail = parser.pos
  end
end

-- Append one capture log entry (parallel arrays, truncated by cap_n rewinds)
local function cap_push(parser, kind, aux, start, len)
  local n = parser.cap_n + 1
  parser.cap_n = n
  parser.cap_kind[n] = kind
  parser.cap_aux[n] = aux
  parser.cap_start[n] = start
  parser.cap_size[n] = len
end

-- Advance past one complete log item (a single entry, or a whole bracketed
-- range including anything nested), returning the index after it
local function cap_skip(parser, i)
  local ck = parser.cap_kind
  local kind = ck[i]
  i = i + 1
  if kind == CAP_TBL_OPEN or kind == CAP_GROUP_OPEN or kind == CAP_FN_OPEN then
    local depth = 1
    while depth > 0 do
      kind = ck[i]
      if kind == CAP_TBL_OPEN or kind == CAP_GROUP_OPEN or kind == CAP_FN_OPEN then
        depth = depth + 1
      elseif kind == CAP_TBL_CLOSE or kind == CAP_GROUP_CLOSE or kind == CAP_FN_CLOSE then
        depth = depth - 1
      end
      i = i + 1
    end
  end
  return i
end

local cap_eval

-- Append the single value a capture group produces to out: its first inner
-- capture value, or the text it matched when its contents produce no values.
-- Returns the index past the group's close entry.
local function cap_eval_group(parser, i, out)
  local open = i
  local after = cap_skip(parser, i)
  local close = after - 1

  local j = open + 1
  while j < close do
    local before_n = out.n
    j = cap_eval(parser, j, out)
    if out.n > before_n then
      -- keep only the first value
      for k = before_n + 2, out.n do out[k] = nil end
      out.n = before_n + 1
      return after
    end
  end

  -- no values: the group's value is the text it matched
  local start = parser.cap_start[open]
  out.n = out.n + 1
  out[out.n] = sub(parser.input, start + 1, parser.cap_start[close])
  return after
end

-- Materialize one log item (entry or bracketed range) at i, appending its
-- values to out (out.n counts values so nil captures are preserved).
-- Returns the index past the item. Runs once after a successful parse (and
-- on demand at Cmt boundaries), so it is not on the matching hot path.
function cap_eval(parser, i, out)
  local ck = parser.cap_kind
  local kind = ck[i]
  if kind == CAP_STR then
    local start = parser.cap_start[i]
    out.n = out.n + 1
    out[out.n] = sub(parser.input, start + 1, start + parser.cap_size[i])
    return i + 1
  elseif kind == CAP_CONST then
    out.n = out.n + 1
    out[out.n] = parser.cap_aux[i]
    return i + 1
  elseif kind == CAP_NIL then
    out.n = out.n + 1
    out[out.n] = nil
    return i + 1
  elseif kind == CAP_POS then
    out.n = out.n + 1
    out[out.n] = parser.cap_start[i] + 1
    return i + 1
  elseif kind == CAP_VALUE then
    out.n = out.n + 1
    out[out.n] = parser.values[parser.cap_aux[i]]
    return i + 1
  elseif kind == CAP_GROUP_OPEN then
    return cap_eval_group(parser, i, out)
  elseif kind == CAP_FN_OPEN then
    -- Transform capture: inner values become arguments, the callback's
    -- return values become the capture values (innermost-first order falls
    -- out of the recursion here)
    local open = i
    local fn = parser.cap_aux[i]
    local args = {n = 0}
    local j = open + 1
    while ck[j] ~= CAP_FN_CLOSE do
      if ck[j] == CAP_GROUP_OPEN then
        -- named groups are not visible as arguments (as at the top level)
        j = cap_skip(parser, j)
      else
        j = cap_eval(parser, j, args)
      end
    end
    if args.n == 0 then
      -- no inner captures: the callback receives the matched text
      local start = parser.cap_start[open]
      args[1] = sub(parser.input, start + 1, parser.cap_start[j])
      args.n = 1
    end
    -- callback errors propagate (abort materialization with the original
    -- error value)
    local rets = pack(fn(unpack(args, 1, args.n)))
    for k = 1, rets.n do
      out.n = out.n + 1
      out[out.n] = rets[k]
    end
    return j + 1
  else -- CAP_TBL_OPEN
    local tbl = {}
    local j = i + 1
    local array_idx = 1
    local item = {n = 0}
    while ck[j] ~= CAP_TBL_CLOSE do
      if ck[j] == CAP_GROUP_OPEN then
        local group_name = parser.cap_aux[j]
        item.n = 0
        j = cap_eval_group(parser, j, item)
        tbl[group_name] = item[1]
      else
        item.n = 0
        j = cap_eval(parser, j, item)
        for k = 1, item.n do
          tbl[array_idx] = item[k]
          array_idx = array_idx + 1
        end
      end
    end
    out.n = out.n + 1
    out[out.n] = tbl
    return j + 1
  end
end
]==]

local CAP_SELECT_HELPER = [==[
-- Reduce the log after base to only the nth capture value (group captures
-- don't count), or to a single nil when there are fewer than n values
local function cap_select(parser, base, n)
  local i = base + 1
  local count = 0
  while i <= parser.cap_n do
    if parser.cap_kind[i] == CAP_GROUP_OPEN then
      i = cap_skip(parser, i)
    else
      local item_start = i
      i = cap_skip(parser, i)
      count = count + 1
      if count == n then
        local ck, ca, cs, cz = parser.cap_kind, parser.cap_aux, parser.cap_start, parser.cap_size
        local len = i - item_start
        for k = 0, len - 1 do
          ck[base + 1 + k] = ck[item_start + k]
          ca[base + 1 + k] = ca[item_start + k]
          cs[base + 1 + k] = cs[item_start + k]
          cz[base + 1 + k] = cz[item_start + k]
        end
        parser.cap_n = base + len
        return
      end
    end
  end
  parser.cap_n = base
  cap_push(parser, CAP_NIL, nil, 0, 0)
end
]==]

local CAP_MATCH_BACK_HELPER = [==[
-- Match the text of the most recent visible named capture group at the
-- current input position. Groups inside completed capture tables are not
-- visible.
local function cap_match_back(parser, name)
  local ck, ca = parser.cap_kind, parser.cap_aux
  local cs, cz = parser.cap_start, parser.cap_size
  local i = parser.cap_n
  while i >= 1 do
    local kind = ck[i]
    if kind == CAP_TBL_CLOSE or kind == CAP_GROUP_CLOSE or kind == CAP_FN_CLOSE then
      local close = i
      local depth = 1
      while depth > 0 do
        i = i - 1
        local k2 = ck[i]
        if k2 == CAP_TBL_CLOSE or k2 == CAP_GROUP_CLOSE or k2 == CAP_FN_CLOSE then
          depth = depth + 1
        elseif k2 == CAP_TBL_OPEN or k2 == CAP_GROUP_OPEN or k2 == CAP_FN_OPEN then
          depth = depth - 1
        end
      end
      if kind == CAP_GROUP_CLOSE and ca[i] == name then
        local text
        local inner = i + 1
        if inner == close then
          -- group captured nothing: its value is the text it matched
          text = sub(parser.input, cs[i] + 1, cs[close])
        elseif ck[inner] == CAP_STR then
          text = sub(parser.input, cs[inner] + 1, cs[inner] + cz[inner])
        elseif ck[inner] == CAP_CONST then
          if type(ca[inner]) ~= "string" then
            return false
          end
          text = ca[inner]
        else
          return false -- group holds a non-string value
        end
        if parser.pos + #text <= parser.input_len and
            sub(parser.input, parser.pos + 1, parser.pos + #text) == text then
          parser.pos = parser.pos + #text
          return true
        end
        return false
      end
    end
    i = i - 1
  end
  return false
end
]==]

local RUN_CMT_HELPER = [==[
local floor = math.floor

-- Run a match-time capture: materialize the inner captures, call the
-- callback with (subject, pos, ...captures), and interpret its results per
-- lpeg semantics: position/true = success, false/nil = failure, extra
-- return values become captures (stored in the parser's values array)
local function run_cmt(parser, fn, start_pos, cap_base)
  local pos_after_inner = parser.pos

  local args = {parser.input, pos_after_inner + 1, n = 2}
  local i = cap_base + 1
  while i <= parser.cap_n do
    if parser.cap_kind[i] == CAP_GROUP_OPEN then
      -- named groups only matter inside Ct; they aren't passed as arguments
      i = cap_skip(parser, i)
    else
      i = cap_eval(parser, i, args)
    end
  end
  parser.cap_n = cap_base -- consume the inner captures

  -- callback errors propagate (abort the parse with the original value)
  local rets = pack(fn(unpack(args, 1, args.n)))

  local ok = false
  if rets.n > 0 then
    local first = rets[1]
    if type(first) == "number" then
      -- number = new position (1-based from Lua), must be in range
      -- [pos_after_inner, input_len]. Floored so a fractional return can't
      -- put the parser on a non-integer position.
      local new_pos = floor(first) - 1
      if new_pos >= pos_after_inner and new_pos <= parser.input_len then
        parser.pos = new_pos
        ok = true
      end
    elseif first == true then
      -- true = succeed without consuming (position stays at pos_after_inner)
      ok = true
    end
  end

  if ok then
    parser.success = true
    if rets.n > 1 then
      local values = parser.values
      local vn = parser.values_n
      for r = 2, rets.n do
        vn = vn + 1
        values[vn] = rets[r]
        cap_push(parser, CAP_VALUE, vn, 0, 0)
      end
      parser.values_n = vn
    end
  else
    parser.success = false
    record_furthest(parser)
    parser.pos = start_pos
  end
end
]==]

local IND_HELPERS = [==[
-- Rewind the indenter trail to a previous length, undoing pushes and pops
local function ind_trail_rewind(parser, index)
  local n = parser.trail_n
  if n <= index then return end
  local trail_id, trail_op, trail_val = parser.trail_id, parser.trail_op, parser.trail_val
  local stacks = parser.ind_stacks
  while n > index do
    local s = stacks[trail_id[n]]
    if trail_op[n] == 0 then
      -- undo push
      s.n = s.n - 1
    else
      -- undo pop: restore the popped value
      s.n = s.n + 1
      s[s.n] = trail_val[n]
    end
    n = n - 1
  end
  parser.trail_n = n
end

local function ind_push(parser, sidx, value)
  local s = parser.ind_stacks[sidx]
  s.n = s.n + 1
  s[s.n] = value
  local tn = parser.trail_n + 1
  parser.trail_n = tn
  parser.trail_id[tn] = sidx
  parser.trail_op[tn] = 0
  parser.trail_val[tn] = value
end

-- Pop the stack; returns false if the stack is empty
local function ind_pop(parser, sidx)
  local s = parser.ind_stacks[sidx]
  local sn = s.n
  if sn == 0 then
    return false
  end
  local value = s[sn]
  s.n = sn - 1
  local tn = parser.trail_n + 1
  parser.trail_n = tn
  parser.trail_id[tn] = sidx
  parser.trail_op[tn] = 1
  parser.trail_val[tn] = value
  return true
end

-- Measure the indentation width of the run of space/tab characters at the
-- current position (space = 1, tab = tab_width). Also returns the first
-- position past the run.
local function ind_measure(parser, tab_width)
  local input, input_len = parser.input, parser.input_len
  local p = parser.pos
  local width = 0
  while p < input_len do
    local c = byte(input, p + 1)
    if c == 32 then
      width = width + 1
    elseif c == 9 then
      width = width + tab_width
    else
      break
    end
    p = p + 1
  end
  return width, p
end
]==]

-- Generate the callback (Cmt/Cfn) loading code, run once at module load
local function generate_cmt_infrastructure(cmt_codes)
  if #cmt_codes == 0 then
    return ""
  end

  local lines = {
    "-- Callback (Cmt/Cfn) infrastructure: load each callback's Lua code once",
    "do",
    "  local load_chunk = loadstring or load"
  }

  for _, cmt in ipairs(cmt_codes) do
    if cmt.kind == "cfn" then
      -- Cfn: the chunk runs once at load and must return the callback
      table.insert(lines, template_code([[  do
    local chunk, load_err = load_chunk($CODE$, "pgen Cfn $ID$")
    if not chunk then
      error("Failed to load Cfn callback $ID$: " .. tostring(load_err))
    end
    local run_ok, fn = pcall(chunk)
    if not run_ok then
      error("Failed to run Cfn chunk $ID$: " .. tostring(fn))
    end
    if type(fn) ~= "function" then
      error("Cfn chunk $ID$ did not return a function")
    end
    cmt_fns[$ID$] = fn
  end]], {ID = cmt.id, CODE = lua_string_literal(cmt.code)}))
    else
      -- Cmt: the loaded chunk IS the callback (invoked with vararg arguments)
      table.insert(lines, template_code([[  do
    local chunk, load_err = load_chunk($CODE$, "pgen Cmt $ID$")
    if not chunk then
      error("Failed to load Cmt callback $ID$: " .. tostring(load_err))
    end
    cmt_fns[$ID$] = chunk
  end]], {ID = cmt.id, CODE = lua_string_literal(cmt.code)}))
    end
  end

  table.insert(lines, "end")
  table.insert(lines, "")

  return table.concat(lines, "\n")
end

-- Generate the character set tables collected during rule generation
local function generate_sets_code(set_list)
  if #set_list == 0 then
    return ""
  end

  local lines = {"-- Character set lookup tables (byte -> true)"}
  for i, set in ipairs(set_list) do
    local entries = {}
    local seen = {}
    for j = 1, #set do
      local b = set:byte(j)
      if not seen[b] then
        seen[b] = true
        entries[#entries + 1] = "[" .. b .. "] = true"
      end
    end
    lines[#lines + 1] = template_code("sets[$IDX$] = { $ENTRIES$ } -- $DISPLAY$", {
      IDX = i,
      ENTRIES = table.concat(entries, ", "),
      DISPLAY = lua_string_literal(set)
    })
  end
  lines[#lines + 1] = ""

  return table.concat(lines, "\n")
end

local function generate_parser_main(start_rule, context, indenters, memo_count)
  local extra_fields = {}

  if context.errors then
    extra_fields[#extra_fields + 1] = 'error_message = "",'
  end

  if memo_count > 0 then
    extra_fields[#extra_fields + 1] = "memo_pos = {}, memo_end = {},"
  end

  if #indenters > 0 then
    local stacks = {}
    for _, ind in ipairs(indenters) do
      stacks[#stacks + 1] = "{ " .. ind.initial .. ", n = 1 }"
    end
    extra_fields[#extra_fields + 1] = "ind_stacks = { " .. table.concat(stacks, ", ") .. " },"
    extra_fields[#extra_fields + 1] = "trail_id = {}, trail_op = {}, trail_val = {}, trail_n = 0,"
  end

  local extra = #extra_fields > 0 and
    ("\n    " .. table.concat(extra_fields, "\n    ")) or ""

  return template_code([[
local function new_parser(input)
  return {
    input = input,
    input_len = #input,
    pos = 0, -- 0-based like the C target; converted at the API boundary
    success = true,
    throw_label = nil, -- label from T() or nil for ordinary failure
    throw_pos = 0,
    furthest_fail = 0,
    depth = 0,
    cap_kind = {}, cap_aux = {}, cap_start = {}, cap_size = {},
    cap_n = 0,
    values = {}, values_n = 0,$EXTRA_FIELDS$
  }
end

local function parse(input)
  if type(input) == "number" then
    input = tostring(input)
  end
  if type(input) ~= "string" then
    error("Expected string argument for parsing")
  end

  local parser = new_parser(input)

  rules[$START_RULE$](parser)

  -- Return nil and error info on failure
  if not parser.success then
    if parser.throw_label then
      -- Labeled failure: return nil, label, position
      return nil, parser.throw_label, parser.throw_pos + 1
    end
    -- Ordinary failure: return nil, message (pgen_errors builds only) and
    -- the furthest input position a match attempt failed at (1-indexed)
    return nil, $FAIL_MESSAGE$, parser.furthest_fail + 1
  end

  -- Materialize the capture log into return values. Named groups produce
  -- no top-level values (they only matter inside Ct).
  local out = {n = 0}
  local i = 1
  local ck = parser.cap_kind
  while i <= parser.cap_n do
    if ck[i] == CAP_GROUP_OPEN then
      i = cap_skip(parser, i)
    else
      i = cap_eval(parser, i, out)
    end
  end

  if out.n > 0 then
    -- Probe large result lists first: unpack past the runtime's stack limit
    -- must surface as a clean, recognizable error
    if out.n >= 1000 and not pcall(unpack, out, 1, out.n) then
      error("pgen: Lua stack overflow while building captures")
    end
    return unpack(out, 1, out.n)
  end

  -- Success case with no captures: return position of consumed input
  return parser.pos + 1
end

return {
  parse = parse
}
]], {
    START_RULE = lua_string_literal(tostring(start_rule)),
    FAIL_MESSAGE = context.errors and "parser.error_message" or "nil",
    EXTRA_FIELDS = extra
  })
end

-- Compile a grammar definition to Lua code
function generator.generate(grammar, parser_name, options)
  options = options or {}
  local pgen_version = options.pgen_version or "unknown"

  local max_depth = 5000
  if options.max_depth then
    assert(type(options.max_depth) == "number" and options.max_depth >= 1,
      "max_depth must be a positive number")
    max_depth = math.floor(options.max_depth)
  end

  -- Collect Cmt/Cfn codes and get transformed grammar with cmt_id assigned
  local cmt_codes, transformed_grammar = common.collect_cmt_codes(grammar)

  -- Collect indenter descriptors and assign stack ids to Ind nodes
  local indenters
  indenters, transformed_grammar = common.collect_indenters(transformed_grammar)

  local rules, start_rule = common.extract_rules(transformed_grammar)

  -- Reject unbounded repetitions whose body can match the empty string,
  -- which would loop forever at parse time
  local analyze = require("pgen.analyze")
  analyze.check_loops(rules)

  -- Assign single-slot memo ids (1-based for Lua arrays) to position-pure
  -- rules, sorted for deterministic output
  local memo_ids = {}
  local memo_count = 0
  do
    local purity = analyze.pure_rules(rules)
    local pure_names = {}
    for name, pure in pairs(purity) do
      if pure then
        table.insert(pure_names, name)
      end
    end
    table.sort(pure_names)
    for i, name in ipairs(pure_names) do
      memo_ids[name] = i
    end
    memo_count = #pure_names
  end

  local context = {
    analyze = analyze,
    rules = rules,
    stateful_rules = analyze.stateful_rules(rules),
    memo_ids = memo_ids,
    errors = options.pgen_errors and true or false,
    has_indenters = #indenters > 0,
    set_index = {},
    set_list = {},
    dispatch_inits = {},
    features = {}
  }

  -- Generate rule functions first: this collects the sets and dispatch
  -- tables referenced by the emitted code
  local rule_chunks = {"-- Rule functions"}
  for name, pattern in sorted_rules(rules, start_rule) do
    rule_chunks[#rule_chunks + 1] = generator.generate_rule_function(name, pattern, context)
  end

  local prelude_lines = {
    template_code([[-- Generated by pgen $PGEN_VERSION$ (Lua target)
-- $PARSER_NAME$ - generated parser
--
-- Self-contained pure Lua module (Lua 5.1+ and LuaJIT), no dependencies.
--   local $PARSER_NAME$ = require "$PARSER_NAME$"
--   local result = $PARSER_NAME$.parse("your input string")

local select, type, error, pcall, tostring = select, type, error, pcall, tostring
local byte, sub = string.byte, string.sub
local unpack = table.unpack or unpack
local pack = table.pack or function(...) return {n = select("#", ...), ...} end

-- Maximum rule-call recursion depth before the parse is aborted with a Lua
-- error (catch with pcall). Configure with the max_depth compile option.
local MAX_DEPTH = $MAX_DEPTH$

-- Capture log entry kinds. Captures are recorded as log entries during
-- matching and only materialized into Lua values after the whole parse
-- succeeds; backtracking rewinds the log length, so discarded speculative
-- captures are never built. The exception is Cmt: its callback runs
-- mid-parse and its extra return values are stored in the parser's values
-- array, referenced by CAP_VALUE entries.
local CAP_STR, CAP_CONST, CAP_NIL, CAP_POS, CAP_VALUE = 1, 2, 3, 4, 5
local CAP_TBL_OPEN, CAP_TBL_CLOSE = 6, 7
local CAP_GROUP_OPEN, CAP_GROUP_CLOSE = 8, 9
local CAP_FN_OPEN, CAP_FN_CLOSE = 10, 11

local rules = {}]], {
      PGEN_VERSION = pgen_version,
      PARSER_NAME = parser_name,
      MAX_DEPTH = max_depth
    })
  }

  if #context.set_list > 0 then
    prelude_lines[#prelude_lines + 1] = "local sets = {}"
  end
  if #context.dispatch_inits > 0 then
    prelude_lines[#prelude_lines + 1] = "local disp = {}"
  end
  if #cmt_codes > 0 then
    prelude_lines[#prelude_lines + 1] = "local cmt_fns = {}"
  end
  if context.has_indenters then
    prelude_lines[#prelude_lines + 1] =
      "-- Sentinel pushed by `prevent`: no measured width compares greater\nlocal IND_PREVENT = math.huge"
  end
  prelude_lines[#prelude_lines + 1] = ""

  local chunks = {
    table.concat(prelude_lines, "\n"),
    CORE_HELPERS
  }

  if context.features.cap_select then
    chunks[#chunks + 1] = CAP_SELECT_HELPER
  end
  if context.features.cap_match_back then
    chunks[#chunks + 1] = CAP_MATCH_BACK_HELPER
  end
  if context.features.run_cmt then
    chunks[#chunks + 1] = RUN_CMT_HELPER
  end
  if context.has_indenters then
    chunks[#chunks + 1] = IND_HELPERS
  end

  chunks[#chunks + 1] = generate_cmt_infrastructure(cmt_codes)
  chunks[#chunks + 1] = generate_sets_code(context.set_list)

  if #context.dispatch_inits > 0 then
    chunks[#chunks + 1] = "-- FIRST-byte dispatch tables\n" ..
      table.concat(context.dispatch_inits, "\n") .. "\n"
  end

  chunks[#chunks + 1] = table.concat(rule_chunks, "\n")
  chunks[#chunks + 1] = generate_parser_main(start_rule, context, indenters, memo_count)

  return table.concat(chunks, "\n")
end

return generator

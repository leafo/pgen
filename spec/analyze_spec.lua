local pgen = require "pgen"
local P, S, V, C, Cc = pgen.P, pgen.S, pgen.V, pgen.C, pgen.Cc

-- Compile-time detection of unbounded repetitions whose body can match the
-- empty string (would hang the generated parser). No C compilation involved.

describe("nullable loop detection", function()
  local function assert_rejected(grammar)
    local ok, err = pcall(pgen.compile, grammar)
    assert.is_false(ok)
    assert.matches("loop body may accept empty string", err)
  end

  local function assert_compiles(grammar)
    local output = pgen.compile(grammar)
    assert.is_string(output)
  end

  it("rejects a zero-width loop body", function()
    assert_rejected{"r", r = P""^0}
  end)

  it("rejects a nullable alternative in a loop", function()
    assert_rejected{"r", r = (P"a" + P"")^1}
  end)

  it("rejects an optional body in a loop", function()
    assert_rejected{"r", r = (P"a"^-1)^0}
  end)

  it("rejects a nullable rule reference in a loop", function()
    assert_rejected{"r", r = V"ws"^1, ws = S" \t"^0}
  end)

  it("rejects a constant capture in a loop", function()
    assert_rejected{"r", r = Cc(1)^0}
  end)

  it("rejects a zero-width indenter operation in a loop", function()
    local ind = pgen.indenter{}
    assert_rejected{"r", r = ind.cpush(1)^0}
  end)

  it("accepts a consuming loop body", function()
    assert_compiles{"r", r = P"a"^0}
  end)

  it("accepts a loop body with a consuming tail", function()
    assert_compiles{"r", r = (S" "^0 * P"a")^0}
  end)

  it("accepts a bounded repetition of a nullable body", function()
    -- at-most loops already break on zero-width matches at runtime
    assert_compiles{"r", r = (S" "^0)^-3 * P"a"}
  end)

  it("accepts recursion that always consumes", function()
    assert_compiles{
      "r",
      r = V"x"^0,
      x = P"(" * V"x" * P")" + P"a",
    }
  end)
end)

describe("backtrack state analysis", function()
  local analyze = require "pgen.analyze"

  local function changes(pattern, rules)
    rules = rules or {}
    return analyze.changes_backtrack_state(pattern, rules, analyze.stateful_rules(rules))
  end

  it("distinguishes capture-free patterns from captures", function()
    assert.is_false(changes(P"a" * S"bc"))
    assert.is_true(changes(P"a" * C(P"b")))
    assert.is_true(changes(Cc("value")))
  end)

  it("resolves state changes through rule references", function()
    local rules = {
      plain = P"a" * V"tail",
      tail = P"b",
      captured = V"value",
      value = C(P"c"),
    }
    assert.is_false(changes(V"plain", rules))
    assert.is_true(changes(V"captured", rules))
  end)

  it("handles capture-free and stateful recursive rule graphs", function()
    local plain = {
      value = P"(" * V"value" * P")" + P"a",
    }
    local stateful = {
      a = V"b" + C(P"a"),
      b = V"a" + P"b",
    }
    assert.is_false(changes(V"value", plain))
    assert.is_true(changes(V"a", stateful))
    assert.is_true(changes(V"b", stateful))
  end)

  it("treats indenter operations and unknown nodes conservatively", function()
    local ind = pgen.indenter{}
    assert.is_true(changes(ind.push))
    assert.is_true(changes({type = "future_pattern_type"}))
    assert.is_true(changes(V"missing", {}))
  end)

  it("emits both input-only and full-state restore paths", function()
    local output = pgen.compile({
      "start",
      start = V"plain" + V"captured",
      plain = P"a" * P"b",
      captured = C(P"c") * P"d",
    }, {optimize = false})
    assert.matches("REMEMBER_INPUT_POSITION%(parser, pos%);", output)
    assert.matches("REMEMBER_POSITION%(parser, pos%);", output)
  end)
end)

describe("position purity analysis", function()
  local analyze = require "pgen.analyze"
  local L, T, Cmb = pgen.L, pgen.T, pgen.Cmb

  local function purity(rules)
    return analyze.pure_rules(rules)
  end

  it("marks capture-free matching rules pure", function()
    local p = purity{
      space = S" \t"^0 * V"comment"^-1,
      comment = P"--" * (P(1) - S"\r\n")^0 * L(V"stop"),
      stop = P"\n" + P(-1),
    }
    assert.is_true(p.space)
    assert.is_true(p.comment)
    assert.is_true(p.stop)
  end)

  it("keeps cycles of pure rules pure", function()
    local p = purity{
      a = P"(" * V"b" * P")" + P"x",
      b = V"a" + P"y",
    }
    assert.is_true(p.a)
    assert.is_true(p.b)
  end)

  it("propagates impurity through rule references", function()
    local p = purity{
      top = P"a" * V"captured",
      captured = C(P"b"),
      plain = P"c",
    }
    assert.is_false(p.top)
    assert.is_false(p.captured)
    assert.is_true(p.plain)
  end)

  it("treats labels, backreferences, and indenters as impure", function()
    local ind = pgen.indenter{}
    local p = purity{
      labeled = P"a" + T"expected_a",
      backref = P"x" * Cmb"eq",
      indented = ind.check * P"a",
    }
    assert.is_false(p.labeled)
    assert.is_false(p.backref)
    assert.is_false(p.indented)
  end)

  it("emits memo slots only for pure rules", function()
    local output = pgen.compile({
      "start",
      start = V"space" * C(V"word"),
      space = S" \t"^0,
      word = pgen.R"az"^1,
    }, {optimize = false})
    assert.matches("PGEN_MEMO_COUNT", output)
    assert.matches("parser%->memo%[", output)
  end)
end)

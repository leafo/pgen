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

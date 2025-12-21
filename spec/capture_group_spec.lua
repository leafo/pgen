describe("capture group (Cg) parser", function()
  local pgen = require "pgen"

  it("creates named fields in Ct", function()
    local parser = pgen.require("spec.parsers.capture_group")

    local result, err = parser.parse("hello world")
    assert.is_nil(err)
    assert.same({
      greeting = "hello",
      "world"
    }, result)
  end)

  it("handles multiple named captures", function()
    local P, Ct, Cg = pgen.P, pgen.Ct, pgen.Cg

    local grammar = {
      "test",
      test = Ct(Cg(P"a", "first") * Cg(P"b", "second"))
    }

    local code = pgen.compile(grammar, {parser_name = "cg_multi"})

    -- Write to temp file and compile
    local tmp_c = os.tmpname() .. ".c"
    local tmp_so = os.tmpname() .. ".so"

    local f = io.open(tmp_c, "w")
    f:write(code)
    f:close()

    local gcc_cmd = string.format(
      "gcc -shared -o %s -O2 -fPIC %s `pkg-config --cflags --libs lua5.1`",
      tmp_so, tmp_c
    )
    assert(os.execute(gcc_cmd) == 0, "Failed to compile")

    local parser = assert(package.loadlib(tmp_so, "luaopen_cg_multi"))()

    local result = parser.parse("ab")
    assert.same({
      first = "a",
      second = "b"
    }, result)

    os.remove(tmp_c)
    os.remove(tmp_so)
  end)

  it("handles mixed named and positional captures", function()
    local P, C, Ct, Cg = pgen.P, pgen.C, pgen.Ct, pgen.Cg

    local grammar = {
      "test",
      test = Ct(C(P"x") * Cg(P"y", "named") * C(P"z"))
    }

    local code = pgen.compile(grammar, {parser_name = "cg_mixed"})

    local tmp_c = os.tmpname() .. ".c"
    local tmp_so = os.tmpname() .. ".so"

    local f = io.open(tmp_c, "w")
    f:write(code)
    f:close()

    local gcc_cmd = string.format(
      "gcc -shared -o %s -O2 -fPIC %s `pkg-config --cflags --libs lua5.1`",
      tmp_so, tmp_c
    )
    assert(os.execute(gcc_cmd) == 0, "Failed to compile")

    local parser = assert(package.loadlib(tmp_so, "luaopen_cg_mixed"))()

    local result = parser.parse("xyz")
    assert.same({
      "x",
      named = "y",
      "z"
    }, result)

    os.remove(tmp_c)
    os.remove(tmp_so)
  end)

  it("works with Cc constant captures", function()
    local P, C, Ct, Cc, Cg = pgen.P, pgen.C, pgen.Ct, pgen.Cc, pgen.Cg

    local grammar = {
      "test",
      test = Ct(Cc("type") * Cg(P"value", "data"))
    }

    local code = pgen.compile(grammar, {parser_name = "cg_cc"})

    local tmp_c = os.tmpname() .. ".c"
    local tmp_so = os.tmpname() .. ".so"

    local f = io.open(tmp_c, "w")
    f:write(code)
    f:close()

    local gcc_cmd = string.format(
      "gcc -shared -o %s -O2 -fPIC %s `pkg-config --cflags --libs lua5.1`",
      tmp_so, tmp_c
    )
    assert(os.execute(gcc_cmd) == 0, "Failed to compile")

    local parser = assert(package.loadlib(tmp_so, "luaopen_cg_cc"))()

    local result = parser.parse("value")
    assert.same({
      "type",
      data = "value"
    }, result)

    os.remove(tmp_c)
    os.remove(tmp_so)
  end)
end)

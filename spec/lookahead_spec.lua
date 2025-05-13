local pgen = require "pgen"

describe("lookahead pattern", function()
  local parser

  before_each(function()
    -- Load the grammar from the parser file
    parser = pgen.require("spec.parsers.lookahead")
  end)

  it("uses lookahead to assert a pattern without consuming it", function()
    -- Parse "abcdef" - this will match if the lookahead works correctly
    -- Since "def" needs to be consumed both by the lookahead and the following pattern
    assert.same({7}, {parser.parse("abcdef")})
  end)

  it("fails if input doesn't match the lookahead pattern", function()
    -- This should fail because "abc" is followed by "xyz", not "def"
    local result, error_msg = parser.parse("abcxyz")
    assert.is_nil(result)
  end)

  it("succeeds with negative lookahead when pattern doesn't follow x", function()
    -- "xyz" is followed by something other than "def"
    assert.same({4}, {parser.parse("xyzghi")})
  end)

  it("fails with negative lookahead when forbidden pattern follows x", function()
    -- "xyz" is followed by "def", not allowed by negative lookahead
    local result, error_msg = parser.parse("xyzdef")
    assert.is_nil(result)
  end)
end)

local json_parser = require("json_parser")

describe("json_parser", function()
  local function parse_and_assert(input, should_succeed)
    assert.are.equal(should_succeed, json_parser.parse(input) ~= nil)
  end

  it("parses simple object", function()
    parse_and_assert('{"key": "value"}', true)
  end)

  it("parses nested object", function()
    parse_and_assert('{"person": {"name": "John", "age": 30}}', true)
  end)

  it("parses array of numbers", function()
    parse_and_assert('[1, 2, 3, 4, 5]', true)
  end)

  it("parses array of objects", function()
    parse_and_assert('[{"key1": "value1"}, {"key2": "value2"}]', true)
  end)

  it("parses empty object", function()
    parse_and_assert('{}', true)
  end)

  it("parses empty array", function()
    parse_and_assert('[]', true)
  end)

  it("parses mixed array", function()
    parse_and_assert('[1, "two", {"key": "value"}, [3, 4]]', true)
  end)

  it("parses boolean true", function()
    parse_and_assert('true', true)
  end)

  it("parses boolean false", function()
    parse_and_assert('false', true)
  end)

  it("parses null value", function()
    parse_and_assert('null', true)
  end)

  it("parses number with decimals", function()
    parse_and_assert('123.456', true)
  end)

  it("parses escaped characters", function()
    parse_and_assert('{"text": "line1\\nline2"}', true)
  end)

  it("parses unicode characters", function()
    parse_and_assert('{"unicode": "\\u1234"}', true)
  end)

  it("returns nil for invalid json", function()
    parse_and_assert('{key: value}', false)
  end)

  it("returns nil for missing comma", function()
    parse_and_assert('{"key1": "value1" "key2": "value2"}', false)
  end)

  it("parses deeply nested object", function()
    parse_and_assert('{"a":{"b":{"c":{"d":{"e":{"f":{"g":{"h":{"i":{"j":{"k":"value"}}}}}}}}}}}', true)
  end)
end)


describe("json_parser", function()
  local pgen = require "pgen"
  local json_parser = pgen.require("spec.parsers.json_parser")

  local function check_parse(input, success_expected)
    local result = json_parser.parse(input)
    if success_expected then
      assert.is_not_nil(result, "Expected successful parse for: " .. input)
    else
      assert.is_nil(result, "Expected parse to fail for: " .. input)
    end
    return result
  end

  describe("basic types", function()
    it("parses objects", function()
      local result = check_parse('{"key": "value"}', true)
      assert.same({
        "json",
        {
          "object",
          {
            "member",
            {"string", "key"},
            {"string", "value"}
          }
        }
      }, result)
    end)

    it("parses arrays", function()
      local result = check_parse('[1, 2, 3]', true)
      assert.same({
        "json",
        {
          "array",
          {"number", "1"},
          {"number", "2"},
          {"number", "3"}
        }
      }, result)
    end)

    it("parses strings", function()
      local result = check_parse('"hello world"', true)
      assert.same({
        "json",
        {
          "string",
          "hello world"
        }
      }, result)
    end)

    it("parses numbers", function()
      local result = check_parse('42', true)
      assert.same({"json", {"number", "42"}}, result)

      -- Check decimal numbers
      result = check_parse('3.14', true)
      assert.same({"json", {"number", "3.14"}}, result)

      -- Check negative numbers
      result = check_parse('-123', true)
      assert.same({"json", {"number", "-123"}}, result)

      -- Check scientific notation
      result = check_parse('1.23e+10', true)
      assert.same({"json", {"number", "1.23e+10"}}, result)
    end)

    it("parses booleans", function()
      local result = check_parse('true', true)
      assert.same({"json", {"boolean", true}}, result)

      result = check_parse('false', true)
      assert.same({"json", {"boolean", false}}, result)
    end)

    it("parses null", function()
      local result = check_parse('null', true)
      assert.same({"json", {"null"}}, result)
    end)

    it("parses empty objects", function()
      local result = check_parse('{}', true)
      assert.same({"json", {"object"}}, result)
    end)

    it("parses empty arrays", function()
      local result = check_parse('[]', true)
      assert.same({"json", {"array"}}, result)
    end)
  end)

  describe("nested structures", function()
    it("parses nested objects", function()
      local result = check_parse('{"person": {"name": "John", "age": 30}}', true)
      assert.same({
        "json",
        {
          "object",
          {
            "member",
            {"string", "person"},
            {
              "object",
              {
                "member",
                {"string", "name"},
                {"string", "John"}
              },
              {
                "member",
                {"string", "age"},
                {"number", "30"}
              }
            }
          }
        }
      }, result)
    end)

    it("parses nested arrays", function()
      local result = check_parse('[[1, 2], [3, 4]]', true)
      assert.same({
        "json",
        {
          "array",
          {
            "array",
            {"number", "1"},
            {"number", "2"}
          },
          {
            "array",
            {"number", "3"},
            {"number", "4"}
          }
        }
      }, result)
    end)

    it("parses mixed nested structures", function()
      local result = check_parse('[{"key": "value"}, [true, null]]', true)
      assert.same({
        "json",
        {
          "array",
          {
            "object",
            {
              "member",
              {"string", "key"},
              {"string", "value"}
            }
          },
          {
            "array",
            {"boolean", true},
            {"null"}
          }
        }
      }, result)
    end)

    it("parses deeply nested structures", function()
      local result = check_parse('{"a":{"b":{"c":{"d":{"e":"value"}}}}}', true)
      assert.same({
        "json",
        {
          "object",
          {
            "member",
            {"string", "a"},
            {
              "object",
              {
                "member",
                {"string", "b"},
                {
                  "object",
                  {
                    "member",
                    {"string", "c"},
                    {
                      "object",
                      {
                        "member",
                        {"string", "d"},
                        {
                          "object",
                          {
                            "member",
                            {"string", "e"},
                            {"string", "value"}
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }, result)
    end)
  end)

  describe("special cases", function()
    it("parses escaped characters in strings", function()
      local result = check_parse('"line1\\nline2\\tindented"', true)
      assert.same({
        "json",
        {
          "string",
          "line1\\nline2\\tindented"
        }
      }, result)
    end)

    it("parses unicode escape sequences", function()
      local result = check_parse('"unicode: \\u1234"', true)
      assert.same({
        "json",
        {
          "string",
          "unicode: \\u1234"
        }
      }, result)
    end)

    it("handles whitespace correctly", function()
      local result = check_parse('   {   "key"   :   "value"   }   ', true)
      assert.same({
        "json",
        {
          "object",
          {
            "member",
            {"string", "key"},
            {"string", "value"}
          }
        }
      }, result)
    end)
  end)

  describe("error cases", function()
    it("rejects missing quotes around object keys", function()
      check_parse('{key: "value"}', false)
    end)

    it("rejects missing commas", function()
      check_parse('{"key1": "value1" "key2": "value2"}', false)
    end)

    it("rejects trailing commas", function()
      check_parse('{"key": "value",}', false)
      check_parse('[1, 2, 3,]', false)
    end)

    it("rejects malformed numbers", function()
      check_parse('01.23', false)
      check_parse('.123', false)
      check_parse('1.', false)
    end)

    it("rejects unclosed structures", function()
      check_parse('{', false)
      check_parse('[', false)
      check_parse('{"key": "value"', false)
      check_parse('["value"', false)
    end)

    it("rejects invalid unicode escapes", function()
      check_parse('"\\u12"', false)
      check_parse('"\\uXYZW"', false)
    end)

    it("rejects unterminated strings", function()
      check_parse('"unterminated', false)
    end)
  end)
end)

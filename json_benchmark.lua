
local MODULE = "examples.json_parser"
MODULE = "spec.parsers.json_parser"

local pgen = require "pgen"
local parser = pgen.require(MODULE)

local lpeg = require "lpeg"
package.loaded.pgen = lpeg
package.loaded[MODULE] = nil
local lpeg_parser = lpeg.P(require(MODULE))

local cjson = require "cjson"

-- Test cases
local test_cases = {
  [[{}]],
  "[]",
  [=[{"name": "test", "values": [1, 2, 3], "active": true}]=],
  [=[{"empty": null, "nested": {"x": 10, "y": 20}}]=],
  [=[["simple", "array"]]=],
  [=[42]=],
  [=[true]=],
  [=["just a string"]=],
  [=[null]=],
  [=[ {"complex": {"nested1": {"flag": false}, "nested2": [1, 2, {"deep": "value"}]}, "array": [10, null, "hello"]} ]=],
  [=[ {"mix": [{"key1": "value1"}, [3.14, false]], "number_string": "123", "boolean_test": [true, false]} ]=],
  [=[ ["mixed", 42, {"json": "object"}, [1, 2, 3]] ]=]
}

local socket = require("socket")
local gettime = socket.gettime

local BENCHMARK = true
local BENCHMARK_COUNT = 10000

local lpeg_time = 0
local pgen_time = 0
local cjson_time = 0

-- Run test cases
for i, test in ipairs(test_cases) do
  print("\nTest case " .. i .. ":")
  print("-----------")
  print("Input: " .. test)

  -- local a = require("moon").dump(assert(parser.parse(test)))
  -- local b = require("moon").dump(assert(lpeg_parser:match(test)))

  -- if a ~= b then
  --   print("Outputs don't match for: " .. test)
  --   print("A: " .. a)
  --   print("B: " .. b)
  --   print()
  -- end

  if BENCHMARK then
    local start = gettime()
    for j = 1, BENCHMARK_COUNT do
      assert(parser.parse(test), "pgen parser failed")
    end
    pgen_time = pgen_time + gettime() - start

    start = gettime()
    for j = 1, BENCHMARK_COUNT do
      assert(lpeg_parser:match(test), "lpeg parser failed")
    end
    lpeg_time = lpeg_time + gettime() - start

    start = gettime()
    for j = 1, BENCHMARK_COUNT do
      cjson.decode(test)
    end
    cjson_time = cjson_time + gettime() - start
  end

  print()
end

if BENCHMARK then
  print("pgen_time", pgen_time)
  print("lpeg_time", lpeg_time)
  print("cjson_time", cjson_time)
end

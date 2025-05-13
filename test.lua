
-- local parser = require("json_parser")
-- print(parser.parse("5"))

local BENCHMARK = os.getenv("BENCHMARK")
local BENCHMARK_COUNT = 10000

local parser = require("calc_parser")

local socket = require("socket")
local gettime = socket.gettime

local lpeg = require "lpeg"
package.loaded.pgen = lpeg
local lpeg_parser = lpeg.P(require("examples.calc_parser"))

local lpeg_time = 0
local pgen_time = 0

local function test(label, ...)
  print("\nTesting " .. label .. "...")
  for i=1,select("#", ...) do
    local input = select(i, ...)
    print("", input .. ":", parser.parse(input))
    print("", "Lpeg:" .. input .. ":", lpeg_parser:match(input))
  end

  if BENCHMARK then
    for i=1,select("#", ...) do
      local input = select(i, ...)

      do
        local start = gettime()
        for i=1,BENCHMARK_COUNT do
          parser.parse(input)
        end
        pgen_time = pgen_time + gettime() - start
      end

      do
        local start = gettime()
        for i=1,BENCHMARK_COUNT do
          lpeg_parser:match(input)
        end
        lpeg_time = lpeg_time + gettime() - start
      end
    end
  end
end


test("basic integer",
  "5",
  "123"
)

test("float", "3.14")

test("addition", "2 + 3")

test("multiplication", "4 * 5")

test("complex expression", "2 * (3 + 4)")

test("whitespace handling",
  "  123  ",
  "  10  +  5  ")

test("nested parentheses", "((2 + 3) * (4 + 5))")

test("negative numbers", "-7")

test("division", "10 / 2")

test("mixed operations", "5 + 3 * 2")

test("invalid input", "3 + * 4")


if BENCHMARK then
  print("pgen_time", pgen_time)
  print("lpeg_time", lpeg_time)
end


-- local parser = require("json_parser")
-- print(parser.parse("5"))

local parser = require("calc_parser")

local function test(label, ...)
  print("\nTesting " .. label .. "...")

  for i=1,select("#", ...) do
    local input = select(i, ...)
    print(input .. ":", parser.parse(input))
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

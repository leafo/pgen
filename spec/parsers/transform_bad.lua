-- Deliberately invalid: a Cfn chunk must return a function. Loading the
-- compiled module raises a Lua error (see transform_capture_spec).
local pgen = require "pgen"

return {
  "test",
  test = pgen.Cfn(pgen.P"x", [[return 42]]),
}

local pgen = require "pgen"
local P, R, S, V, C, Ct, Cp, Cc = pgen.P, pgen.R, pgen.S, pgen.V, pgen.C, pgen.Ct, pgen.Cp, pgen.Cc

-- Grammar demonstrating multiple constants of different types
return {
  "main",

  -- nil in the middle to test capturing empty slot
  main = P"test" * Cc(42) * Cc("test_field") * Cc(nil) * Cc(true),
}

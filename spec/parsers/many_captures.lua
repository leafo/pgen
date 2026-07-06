local pgen = require "pgen"
local P, V, C, Ct = pgen.P, pgen.V, pgen.C, pgen.Ct

-- Produces one capture per input character to exercise Lua stack growth and
-- the clean overflow error from pgen_checkstack

return {
  "test",

  test = P"1:" * V"flat" +
         P"2:" * V"collected",

  -- captures accumulate directly on the Lua stack
  flat = C(P"a")^0,

  -- captures accumulate on the stack until the table is built
  collected = Ct(C(P"a")^0),
}

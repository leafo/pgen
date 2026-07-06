-- MoonScript parser built on pgen.
--
--   local parse = require "moonscript_pgen"
--   local tree, err = parse.string(moon_code)
--
-- The parser is compiled (grammar -> C -> .so via gcc) on first use and
-- cached for the lifetime of the process. Returns the same AST shapes as
-- the reference parser in moonscript/moonscript/parse.lua.

local pgen = require "pgen"
local tree = require "moonscript_pgen.tree"

local parse = {}

local parser

local function get_parser()
  if not parser then
    parser = pgen.require("moonscript_pgen.grammar", {
      parser_name = "moonscript_parser",
    })
  end
  return parser
end

function parse.string(str)
  local raw = get_parser().parse(str)
  if not raw then
    return nil, "Failed to parse"
  end

  local ok, result = pcall(tree.normalize, raw)
  if not ok then
    if type(result) == "table" then
      -- error({node, msg}) raised from format_assign, like the reference
      local _, msg = result[1], result[2]
      return nil, "Failed to parse: " .. tostring(result[2])
    end
    error(result, 0)
  end

  return result
end

return parse

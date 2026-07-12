-- MoonScript parser built on pgen.
--
--   local parse = require "moonscript_pgen"
--   local tree, err = parse.string(moon_code)
--
-- The parser is compiled (grammar -> C -> .so via gcc) on first use and
-- cached for the lifetime of the process. Returns the same AST shapes as
-- the reference parser in moonscript/moonscript/parse.lua.
--
-- On failure, err is a line/column message. The position comes from the
-- parser's furthest-failure tracking, or from the offending node for
-- assignment errors found during normalization.

local pgen = require "pgen"
local errors = require "pgen.errors"

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
  local parser = get_parser()
  -- Grammar transforms run during parse(); format_assign raises
  -- error({node, msg}) for invalid assignment targets, like the reference
  local ok, result, label, err_pos = pcall(parser.parse, str)
  if not ok then
    if type(result) == "table" then
      local node, msg = result[1], result[2]
      local pos = type(node) == "table" and node[-1]
      if pos then
        return nil, errors.format(str, pos, msg)
      end
      return nil, "failed to parse: " .. tostring(msg)
    end
    error(result, 0)
  end

  if not result then
    if err_pos then
      return nil, errors.format(str, err_pos, label or "failed to parse")
    end
    return nil, "failed to parse"
  end

  return result
end

return parse

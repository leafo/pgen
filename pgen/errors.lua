local errors = {}

-- Calculate line number and column from byte position (local helper)
-- Returns: line (1-indexed), column (1-indexed)
local function calcline(subject, pos)
  if pos <= 1 then return 1, 1 end
  local sub = subject:sub(1, pos - 1)
  local line = 1
  local last_newline = 0
  for i = 1, #sub do
    if sub:sub(i, i) == "\n" then
      line = line + 1
      last_newline = i
    end
  end
  local col = pos - last_newline
  return line, col
end

-- Get the line of text containing the given position (local helper)
-- Returns: line text, column position within that line
local function getline(subject, pos)
  -- Find start of line
  local line_start = pos
  while line_start > 1 and subject:sub(line_start - 1, line_start - 1) ~= "\n" do
    line_start = line_start - 1
  end
  -- Find end of line
  local line_end = pos
  while line_end <= #subject and subject:sub(line_end, line_end) ~= "\n" do
    line_end = line_end + 1
  end
  return subject:sub(line_start, line_end - 1), pos - line_start + 1
end

-- Format a complete error message
-- subject: the input string being parsed
-- pos: byte position where error occurred (1-indexed)
-- label: error label from T() or nil
-- opts: optional table with:
--   color: boolean - if true, use ansicolors for colored output
function errors.format(subject, pos, label, opts)
  local line, col = calcline(subject, pos)
  local line_text, col_in_line = getline(subject, pos)

  local msg
  if opts and opts.color then
    local colors = require("ansicolors")
    msg = colors("%{bright red}" .. (label or "error") .. "%{reset}")
    msg = msg .. colors(" %{dim}at line " .. line .. ", column " .. col .. ":%{reset}\n")
    msg = msg .. "  " .. line_text .. "\n"
    msg = msg .. "  " .. string.rep(" ", col_in_line - 1)
    msg = msg .. colors("%{bright red}^%{reset}")
  else
    msg = string.format("%s at line %d, column %d:\n",
      label or "error", line, col)
    msg = msg .. "  " .. line_text .. "\n"
    msg = msg .. "  " .. string.rep(" ", col_in_line - 1) .. "^"
  end

  return msg
end

return errors

-- Differential tests: the pgen MoonScript parser must produce trees
-- identical to the reference LPeg parser (moonscript/moonscript/parse.lua),
-- including [-1] position annotations, over every .moon file in the
-- moonscript/ checkout plus a battery of targeted snippets.
--
-- Requires the reference checkout at ./moonscript and a working lpeg;
-- the whole suite is skipped when either is unavailable.

local function load_reference()
  local prev_path = package.path
  package.path = "moonscript/?.lua;moonscript/?/init.lua;" .. package.path
  local ok, ref = pcall(require, "moonscript.parse")
  package.path = prev_path
  if ok then
    return ref
  end
end

-- deep comparison over the union of keys; returns diverging path or nil
local function tree_diff(a, b, path)
  path = path or "root"
  if type(a) ~= type(b) then
    return ("%s: type %s vs %s (%s vs %s)"):format(path, type(a), type(b), tostring(a), tostring(b))
  end
  if type(a) ~= "table" then
    if a ~= b then
      return ("%s: %s vs %s"):format(path, tostring(a), tostring(b))
    end
    return nil
  end
  local keys = {}
  for k in pairs(a) do keys[k] = true end
  for k in pairs(b) do keys[k] = true end
  for k in pairs(keys) do
    local d = tree_diff(a[k], b[k], path .. "." .. tostring(k))
    if d then return d end
  end
  return nil
end

local snippets = {
  ["anonymous class"] = "x = class",
  ["anonymous class extends"] = "x = class extends Base",
  ["class extends prevents indent"] = "class A extends B\n  new: =>\n    super!",
  ["class dot target"] = "class a.b.Foo",
  ["nested interpolation"] = [[x = "a#{ "b#{c}d" }e"]],
  ["interpolation with lua string"] = [=[x = "a#{ [[long]] }b"]=],
  ["lua string levels"] = "x = [==[he]]llo]=]x]==]",
  ["lua string leading break"] = "x = [[\nhello]]",
  ["lua string invoke"] = "f[[arg]]",
  ["string escapes"] = [[x = 'it\'s \\'
y = "say \"hi\""]],
  ["unparseable interpolation is content"] = [[x = "#{}"]],
  ["slices"] = "a = y[1,2,3]\nb = y[,2]\nc = y[i+1,]",
  ["do expression"] = "x = do\n  5",
  ["do disabled in headers"] = "while do_it\n  print 1",
  ["statement decorators"] = "print x if y else z\nprint x unless y\nprint x for x in *items",
  ["import"] = "import a, \\b from c",
  ["export"] = "export *\nexport class Foo\nexport a, b = 1, 2",
  ["update operators"] = "x += 1\nx ..= 'a'\nx or= 5\nx >>= 2",
  ["self assign shorthand"] = "t = {:name, :value}",
  ["self names"] = "@x = 1\n@@y = 2\nf = => @",
  ["function args"] = "f = (a=1, b='x', ...) -> a\ng = (a using nil) -> a",
  ["colon chains"] = "x = a\\b\\c\na\\b(1)\\c 2",
  ["with lone chains"] = "with x\n  .y = 1\n  \\foo!",
  ["multiline table"] = "t = {\n  1, 2\n  3\n  a: 4\n}",
  ["table blocks"] = "t =\n  a: 1\n  b:\n    c: 2",
  ["arg blocks"] = "f 1,\n  2, 3\nf a,\n  b: 1\n  c: 2",
  ["numbers"] = "a = 0xff\nb = 1.5e-3\nc = .5\nd = 0xffULL\ne = 10LL",
  ["operator line continuation"] = "x = a +\nb",
  ["comprehensions"] = "x = [i*2 for i in *t when i > 2]\ny = {k, v for k, v in pairs t}\nz = [i for i=1,10]",
  ["switch"] = "x = switch y\n  when 1 then 'a'\n  else 'b'",
  ["if assign condition"] = "if x = get!\n  print x",
  ["crlf line endings"] = "x = 5\r\nif x\r\n  print x\r\n",
  ["tab indentation"] = "if x\n\tprint 1\n\tprint 2",
  ["leading whitespace first line"] = "  x = 5",
  ["shebang"] = "#!/usr/bin/env moon\nx = 5",
  ["keyword-prefixed names"] = "iffy = 1\nformat = 2\ndoor = 3\nnotes = 4\norange = 5",
  -- string content the parser attempts to parse as code while trying
  -- alternatives (these are why the grammar has no T() labels)
  ["statement keywords inside long string"] = "y = [[if x then import a]] .. z",
  ["level-shifted long string open"] = "x = [[=[hi]] .. 1",
  ["quote inside long string content"] = [=[x = y .. [[extend("]] .. z .. [[")]] .. w]=],
  ["keyword as name fails"] = "if = 5",
  ["bad indent fails"] = "if x\nprint y",
  ["non-assignable lhs fails"] = "f! = 5",
}

describe("moonscript_pgen differential", function()
  local ref_parse = load_reference()

  if not ref_parse then
    pending("reference parser unavailable (needs ./moonscript checkout + lpeg)")
    return
  end

  local pgen_parse

  setup(function()
    pgen_parse = require "moonscript_pgen.init"
  end)

  local function assert_same_parse(code, label)
    local ref_tree = ref_parse.string(code)
    local our_tree, our_err = pgen_parse.string(code)
    if ref_tree and not our_tree then
      assert(false, ("%s: reference parses but pgen fails: %s"):format(label, tostring(our_err)))
    elseif not ref_tree and our_tree then
      assert(false, ("%s: reference fails but pgen parses"):format(label))
    elseif ref_tree and our_tree then
      local d = tree_diff(ref_tree, our_tree)
      if d then
        assert(false, ("%s: tree mismatch at %s"):format(label, d))
      end
    end
    -- both failing counts as agreement
  end

  describe("snippets", function()
    local names = {}
    for name in pairs(snippets) do names[#names + 1] = name end
    table.sort(names)

    for _, name in ipairs(names) do
      it(name, function()
        assert_same_parse(snippets[name], name)
      end)
    end
  end)

  describe("corpus", function()
    local files = {}
    local p = io.popen("find moonscript -name '*.moon' | sort")
    if p then
      for line in p:lines() do
        files[#files + 1] = line
      end
      p:close()
    end

    it("finds the corpus", function()
      assert.is_true(#files > 0, "no .moon files found under moonscript/")
    end)

    for _, file in ipairs(files) do
      it(file, function()
        local f = assert(io.open(file))
        local code = f:read("*a")
        f:close()
        assert_same_parse(code, file)
      end)
    end
  end)
end)

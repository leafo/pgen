package = "pgen"
version = "dev-1"

source = {
  url = "git+https://github.com/leafo/pgen.git"
}

description = {
  summary = "Parser generator that compiles LPeg-like grammars into C Lua modules",
  detailed = [[
    PGen takes LPeg-like pattern definitions written in Lua and generates a
    standalone Lua module in C for parsing strings. Generated parsers have
    no runtime dependencies beyond Lua itself. Includes captures, match-time
    Lua callbacks, labeled failures, and indentation-sensitive parsing via
    transactional match-time stacks.
  ]],
  homepage = "https://github.com/leafo/pgen",
  license = "MIT"
}

dependencies = {
  "lua >= 5.1",
  "argparse >= 0.7",
  "lua-cjson"
}

build = {
  type = "builtin",
  modules = {
    ["pgen"] = "pgen.lua",
    ["pgen.analyze"] = "pgen/analyze.lua",
    ["pgen.debug"] = "pgen/debug.lua",
    ["pgen.errors"] = "pgen/errors.lua",
    ["pgen.generator"] = "pgen/generator.lua",
    ["pgen.optimize"] = "pgen/optimize.lua",
    ["pgen.types"] = "pgen/types.lua",
    ["pgen.visitor"] = "pgen/visitor.lua"
  },
  install = {
    bin = {
      pgen = "pgen_cli.lua"
    }
  }
}

.PHONY: test busted local generate-spec-parsers clean-spec-parsers
.SECONDARY:

CLANG_FORMAT_ARGS = -style="{ColumnLimit: 0}"
PGEN_LUA_FILES = pgen.lua $(wildcard pgen/*.lua)
LUA ?= lua
PGEN_CC ?= gcc
PGEN_LUA_CFLAGS ?= $(shell pkg-config --cflags lua5.1)
PGEN_LUA_LIBS ?= $(shell pkg-config --libs lua5.1)

test: calc_parser.so
	$(LUA) test.lua

local:
	luarocks --local --lua-version=5.1 make pgen-dev-1.rockspec

busted: calc_parser.so generate-spec-parsers
	busted spec

# Run the same suite against the pure Lua code generation target (no C
# compiler involved for the parsers under test)
busted-lua: calc_parser.so
	PGEN_TARGET=lua busted spec

parser.so: examples/numbers.lua
	$(LUA) pgen_cli.lua $<
	clang-format $(CLANG_FORMAT_ARGS) -i parser.c
	$(PGEN_CC) -shared -o parser.so -O3 -fPIC parser.c $(PGEN_LUA_CFLAGS) $(PGEN_LUA_LIBS)

%.so: %.c
	$(PGEN_CC) -shared -o $@ -O3 -fPIC $< $(PGEN_LUA_CFLAGS) $(PGEN_LUA_LIBS)

%.c: examples/%.lua pgen/generator.lua
	$(LUA) pgen_cli.lua -o $@ -n $* $<
	clang-format $(CLANG_FORMAT_ARGS) -i $@

generate-spec-parsers: $(patsubst spec/parsers/%.lua, spec/parsers/%.so, $(wildcard spec/parsers/*.lua))

clean-spec-parsers:
	rm spec/parsers/*.{c,so}

spec/parsers/%.c: spec/parsers/%.lua $(PGEN_LUA_FILES)
	$(LUA) pgen_cli.lua -o $@ -n $* $< --pgen-errors
	clang-format $(CLANG_FORMAT_ARGS) -i $@

# the teal parser grammar is a shim that loads the example grammar
spec/parsers/teal_parser.c: examples/teal_parser.lua

spec/parsers/%.so: spec/parsers/%.c
	$(PGEN_CC) -shared -o $@ -O3 -fPIC $< $(PGEN_LUA_CFLAGS) $(PGEN_LUA_LIBS)


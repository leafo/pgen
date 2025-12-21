.PHONY: test busted generate-spec-parsers clean-spec-parsers
.SECONDARY:

CLANG_FORMAT_ARGS = -style="{ColumnLimit: 0}"
PGEN_LUA_FILES = pgen.lua $(wildcard pgen/*.lua)

test: calc_parser.so
	lua5.1 test.lua

busted: calc_parser.so generate-spec-parsers
	busted spec

parser.so: examples/numbers.lua
	./pgen_cli.lua $<
	clang-format $(CLANG_FORMAT_ARGS) -i parser.c
	gcc -shared -o parser.so -O3 -fPIC parser.c `pkg-config --cflags --libs lua5.1`

%.so: %.c
	gcc -shared -o $@ -O3 -fPIC $< `pkg-config --cflags --libs lua5.1`

%.c: examples/%.lua pgen/generator.lua
	./pgen_cli.lua -o $@ -n $* $<
	clang-format $(CLANG_FORMAT_ARGS) -i $@

generate-spec-parsers: $(patsubst spec/parsers/%.lua, spec/parsers/%.so, $(wildcard spec/parsers/*.lua))

clean-spec-parsers:
	rm spec/parsers/*.{c,so}

spec/parsers/%.c: spec/parsers/%.lua $(PGEN_LUA_FILES)
	./pgen_cli.lua --no-optimize -o $@ -n $* $< --pgen-errors
	clang-format $(CLANG_FORMAT_ARGS) -i $@

spec/parsers/%.so: spec/parsers/%.c
	gcc -shared -o $@ -O3 -fPIC $< `pkg-config --cflags --libs lua5.1`


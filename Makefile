

.PHONY: test busted

test: calc_parser.so
	lua5.1 test.lua

busted: json_parser.so calc_parser.so
	busted spec

parser.so: examples/numbers.lua
	./pgen_cli.lua $<
	clang-format -i parser.c
	gcc -shared -o parser.so -O3 -fPIC parser.c `pkg-config --cflags --libs lua5.1`

%.so: %.c
	gcc -shared -o $@ -O3 -fPIC $< `pkg-config --cflags --libs lua5.1`

%.c: examples/%.lua pgen/generator.lua
	./pgen_cli.lua -o $@ -n $* $<
	clang-format -i $@

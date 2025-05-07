

.PHONY: test

test:
	make -B calc_parser.so && lua5.1 test.lua

calc_parser.so: calc_parser.c
	gcc -shared -o calc_parser.so -fPIC calc_parser.c `pkg-config --cflags --libs lua5.1`

calc_parser.c: examples/calculator.lua
	./pgen_cli.lua -o calc_parser.c -n calc_parser examples/calculator.lua
	clang-format -i calc_parser.c


json_parser.so: json_parser.c
	gcc -shared -o json_parser.so -fPIC json_parser.c `pkg-config --cflags --libs lua5.1`

json_parser.c: examples/json_grammar.lua
	./pgen_cli.lua -o json_parser.c -n json_parser examples/json_grammar.lua
	clang-format -i json_parser.c


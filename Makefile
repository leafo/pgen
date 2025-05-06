
calc_parser.so: calc_parser.c
	gcc -shared -o calc_parser.so -fPIC calc_parser.c `pkg-config --cflags --libs lua5.1`

calc_parser.c: examples/calculator.lua
	./pgen_cli.lua -o calc_parser.c -n calc_parser examples/calculator.lua
	clang-format -i calc_parser.c




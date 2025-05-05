
calc_parser: calc_parser.c
	gcc -o calc_parser calc_main.c

calc_parser.o: calc_parser.c
	gcc -c calc_parser.c

calc_parser.c: examples/calculator.lua
	./pgen_cli.lua -o calc_parser.c -n calc_parser examples/calculator.lua
	clang-format -i calc_parser.c




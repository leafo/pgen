#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include generated parser
#include "calc_parser.c"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s \"expression\"\n", argv[0]);
        printf("Example: %s \"2 + 3 * 4\"\n", argv[0]);
        return 1;
    }
    
    const char* input = argv[1];
    printf("Parsing expression: %s\n", input);
    
    if (calc_parser_parse(input)) {
        printf("Expression is valid\n");
        return 0;
    } else {
        printf("Expression is invalid\n");
        return 1;
    }
}
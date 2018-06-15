
#define ASSEMBLER_H


int pass_one(FILE *input, FILE* output, SymbolTable* symtbl);

int pass_two(FILE *input, FILE* output, SymbolTable* symtbl, SymbolTable* reltbl);


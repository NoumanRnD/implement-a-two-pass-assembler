 
#include "header.h"


const int MAX_ARGS = 3;
const int BUF_SIZE = 1024;
const char* IGNORE_CHARS = " \f\n\r\t\v,()";
 
 
static void raise_label_error(uint32_t input_line, const char* label) {
    write_to_log("  - invalid label at line %d: %s\n", input_line, label);
}

 
static void raise_extra_arg_error(uint32_t input_line, const char* extra_arg) {
    write_to_log("  - extra argument at line %d: %s\n", input_line, extra_arg);
}

 
static void raise_inst_error(uint32_t input_line, const char* name, char** args,
    int num_args) {
    
    write_to_log("Error - invalid instruction at line %d: ", input_line);
    log_inst(name, args, num_args);
}

 
static void skip_comment(char* str) {
    char* comment_start = strchr(str, '#');
    if (comment_start) {
        *comment_start = '\0';
    }
}

 
static int add_if_label(uint32_t input_line, char* str, uint32_t byte_offset,
    SymbolTable* symtbl) {
    
    size_t len = strlen(str);
    if (str[len - 1] == ':') {
        str[len - 1] = '\0';
        if (is_valid_label(str)) {
            if (add_to_table(symtbl, str, byte_offset) == 0) {
                return 1;
            } else {
                return -1;
            }
        } else {
            raise_label_error(input_line, str);
            return -1;
        }
    } else {
        return 0;
    }
}

 
int AssemblyPassOne(FILE* input, FILE* output, SymbolTable* symtbl) {
    uint32_t line_counter = 1;
    int numValidInstructSoFar = 0;
    char buf[BUF_SIZE];
    int boolean = 0;
    while (fgets(buf, sizeof(buf), input) ) {
        char* args[MAX_ARGS + 1];
        int num_args = 0;
        skip_comment(buf);
        int toWrite = 0;
        char* currToken = strtok(buf, IGNORE_CHARS);
        if(strcmp(buf, "") == 0){
            line_counter += 1;
            continue;
        }
        if (currToken == NULL) {
            line_counter += 1;
            continue;
        }
        int retval = add_if_label(line_counter, currToken, numValidInstructSoFar * 4, symtbl);
        if(retval != 0) {
            currToken = strtok(NULL, IGNORE_CHARS);
            if(strcmp(currToken, "") == 0){
                line_counter += 1;
                continue;
            }
        } else {
            char name[10];
            strcpy(name, currToken);
            // printf("%s\n", name);
            currToken = strtok(NULL, IGNORE_CHARS);
            while(currToken != NULL){
                char* ptr = currToken;
                args[num_args] = ptr;
                num_args += 1;
                currToken = strtok(NULL, IGNORE_CHARS);
                if(num_args > MAX_ARGS) {
                    boolean = 1;
                    toWrite = 1;
                    raise_extra_arg_error(line_counter, currToken);
                    break;
                }
            }
            int returnVal = write_pass_one(output, name, args, num_args);
            if(!returnVal) {
                boolean = 1;
            }
            if(returnVal && toWrite == 0) {
                numValidInstructSoFar += returnVal;
            }
            line_counter += 1;
        }
    }
    return boolean;
}

 
int AssemblyPassTwo(FILE *input, FILE* output, SymbolTable* symtbl, SymbolTable* reltbl) {
    char buf[BUF_SIZE];
    int boolean = 0;
    uint32_t line = 0;
    char* args[MAX_ARGS];
    while (fgets(buf, sizeof(buf), input)) {
        char* currLine = strtok(buf, IGNORE_CHARS); //MAY NEED TO CHANGE THIS
        int num_args = 0;
        char name[10];
        strcpy(name, currLine);
        currLine = strtok(NULL, IGNORE_CHARS);
        while(currLine != NULL) {
            char* pt = currLine;
            args[num_args] = pt;
            num_args += 1;
            currLine = strtok(NULL, IGNORE_CHARS);
        }
        uint32_t branchOff = line * 4;
        int retval = translate_inst(output, name, args, num_args, branchOff, symtbl, reltbl);
        if (retval == -1) {
            raise_inst_error(line + 1, name, args, num_args);
            boolean = 1;
        }
        line += 1;
    }
    if (boolean) {
        return -1;
    }
    return 0;
}

 
static int open_files(FILE** input, FILE** output, const char* input_name, 
    const char* output_name) {
    
    *input = fopen(input_name, "r");
    if (!*input) {
        write_to_log(" : unable to open input file: %s\n", input_name);
        return -1;
    }
    *output = fopen(output_name, "w");
    if (!*output) {
        write_to_log(" : unable to open output file: %s\n", output_name);
        fclose(*input);
        return -1;
    }
    return 0;
}

static void close_files(FILE* input, FILE* output) {
    fclose(input);
    fclose(output);
}

 
int assemble(const char* in_name, const char* tmp_name, const char* out_name) {
    FILE *src, *dst;
    int err = 0;
    SymbolTable* symtbl = create_table(SYMTBL_UNIQUE_NAME);
    SymbolTable* reltbl = create_table(SYMTBL_NON_UNIQUE);

    if (in_name) {
        printf("Running pass one: %s -> %s\n", in_name, tmp_name);
        if (open_files(&src, &dst, in_name, tmp_name) != 0) {
            free_table(symtbl);
            free_table(reltbl);
            exit(1);
        }

        if (AssemblyPassOne(src, dst, symtbl) != 0) {
            err = 1;
        }
        close_files(src, dst);
    }

    if (out_name) {
        printf("Running pass two: %s -> %s\n", tmp_name, out_name);
        if (open_files(&src, &dst, tmp_name, out_name) != 0) {
            free_table(symtbl);
            free_table(reltbl);
            exit(1);
        }

        fprintf(dst, ".text\n");
        if (AssemblyPassTwo(src, dst, symtbl, reltbl) != 0) {
            err = 1;
        }
        
        fprintf(dst, "\n.symbol\n");
        write_table(symtbl, dst);

        fprintf(dst, "\n.relocation\n");
        write_table(reltbl, dst);

        close_files(src, dst);
    }
    
    free_table(symtbl);
    free_table(reltbl);
    return err;
}

 

int main(int argc, char **argv) {
    

    

    char *input, *inter, *output;
    input = "c.asm";
    inter = "out.txt";
    output = "report.txt";

    int err = assemble(input, inter, output);

    if (err) {
        write_to_log("  errors   during assembly operation.\n");
    } else {
        write_to_log(" completed successfully.\n");
    }

    if (is_log_file_set()) {
        printf("Results saved to %s\n", argv[5]);
    }

    return err;
}
 


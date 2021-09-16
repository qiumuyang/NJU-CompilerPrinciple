#include "assemble.h"
#include "header.h"
#include "ir.h"

int main(int argc, char** argv) {
    if (argc <= 2) return 1;

    // initialize input file pointer
    FILE* fin = fopen(argv[1], "r");
    if (!fin) {
        perror(argv[1]);
        return 1;
    }

    // construct syntax tree
    yyrestart(fin);
    if (yyparse() == 0 && !yyperr) {
        // output syntax tree
        // printTree(root, 0);  // Lab-1

        // run semantic check
        // semantic();          // Lab-2
    }
    if (unhandled > 0) {
        fprintf(stderr, "%s.\n", errmsgB[SYN_ERR]);
    }

    interCode** codes = interCodeGenerate();

    // initialize output file pointer
    FILE* fout = fopen(argv[2], "w");
    if (!fout) {
        perror(argv[2]);
        return 1;
    }

    // generate intermediate code
    // interCodeOutput(fout, codes);  // Lab-3

    // generate assembly code
    assembleGenerate(fout, codes);  // Lab-4

    return 0;
}
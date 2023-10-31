#include <cosmo.h>
#include <stdio.h>
#include <stdlib.h>
#include "types.h"

void readInput(c8 *input) {
    FILE *fp;
    c8 *line = NULL;
    usize lineLen;

    if (!(fp = fopen(input, "r"))) {
        puts("Failed to open file, ensure it exists.");
        exit(1);
    }

    while (line = chomp(fgetln(fp, NULL))) {
        printf("%s\n", line);
    }

    if (ferror(fp) || !feof(fp)) {
        puts("Failed to process file.");
        exit(1);
    }

    fclose(fp);
}

i32 main(i32 argc, c8 *argv[]) {
    if (argc != 2) {
        return 1;
    }

    readInput(argv[1]);

    return 0;
}

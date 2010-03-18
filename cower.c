#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>

#include "aur.h"
#include "package.h"
#include "curlhelper.h"


void usage(void) {
    fprintf(stderr, "usage: cower TYPE PACKAGE\n\n");
}

int main(int argc, char **argv) {

    if(argc != 3) {
        usage();
        return 1;
    }

    if (! strcmp(argv[1], "info")) {
        struct aurpkg *result = aur_pkg_info(argv[2]);
        if (result != NULL) {
            print_package(result);
        } else {
            fprintf(stderr, "Error: Package %s not found\n", argv[2]);
        }
        free(result);
    } else if (! strcmp(argv[1], "search")) {
        aur_pkg_search(argv[2]);
    } else {
        usage();
    }

    return 0;
}


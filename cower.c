#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <jansson.h>

#include "linkedList.h"
#include "curlhelper.h"
#include "aur.h"
#include "package.h"

static llist *pkg_list;

static int parseargs(int argc, char **argv, int *oper_mask, int *opt_mask) {
    int opt;
    int option_index = 0;
    static struct option opts[] = {
        {"search",      no_argument,        0, 's'},
        {"update",      no_argument,        0, 'u'},
        {"info",        no_argument,        0, 'i'},
        {"download",    no_argument,        0, 'd'},
        {"color",       no_argument,        0, 'c'},
        {"verbose",     no_argument,        0, 'v'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "suidcv", opts, &option_index))) {
        if (opt < 0) {
            break;
        }

        switch (opt) {
            case 's':
                *oper_mask |= 1;
                break;
            case 'u':
                *oper_mask |= 2;
                break;
            case 'i':
                *oper_mask |= 4;
                break;
            case 'd':
                *oper_mask |= 8;
                break;
            case 'c':
                *opt_mask |= 1;
                break;
            case 'v':
                *opt_mask |= 2;
                break;
            case '?': 
                return 1;
            default: 
                return 1;
        }
    }

    while (optind < argc) {
        llist_add(&pkg_list, strdup(argv[optind]));
        printf("Package argument found: %s\n", argv[optind]);
        optind++;
    }
}

int main(int argc, char **argv) {

    int ret = 0;
    int oper_mask = 0, opt_mask = 0;

    ret = parseargs(argc, argv, &oper_mask, &opt_mask);

    printf("ret = %d\nOperMask = %d\nOptMask = %d\n", ret, oper_mask, opt_mask);

    return 0;
}


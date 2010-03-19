#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <jansson.h>

#include "util.h"
#include "linkedList.h"
#include "curlhelper.h"
#include "aur.h"
#include "package.h"

#define PKG_URL "http://aur.archlinux.org/packages/%s/%s.tar.gz"

static llist *pkg_list;

static int parseargs(int argc, char **argv, int *oper_mask, int *opt_mask) {
    int opt;
    int option_index = 0;
    static struct option opts[] = {
        /* Operations */
        {"search",      no_argument,        0, 's'},
        {"update",      no_argument,        0, 'u'},
        {"info",        no_argument,        0, 'i'},
        {"download",    no_argument,        0, 'd'},

        /* Options */
        {"color",       no_argument,        0, 'c'},
        {"verbose",     no_argument,        0, 'v'},
        {"force",       no_argument,        0, 'f'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "suidcvf", opts, &option_index))) {
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
            case 'f':
                *opt_mask |= 4;
                break;
            case '?': 
                return 1;
            default: 
                return 1;
        }
    }

    while (optind < argc) {
        llist_add(&pkg_list, strdup(argv[optind]));
        optind++;
    }

    return 0;
}

void usage() {
    printf("Usage: cower [options] <operation> PACKAGE [PACKAGE2..]\n");
    printf("\n");
    printf(" Operations:\n");
    printf("  -d, --download          download PACKAGE(s)\n");
    printf("  -i, --info              show info for PACKAGE(s)\n");
    printf("  -s, --search            search for PACKAGE(s)\n");
/*
    printf("  -u, --update            check explicitly installed packages for available\n");
    printf("                          updates\n");
    printf("                             if passed with --download flag(s), perform download\n");
    printf("                             operation for each package with an available update\n");
*/
    printf("\n");
    printf(" General options:\n");
    printf("  -c, --color             use colored output\n");
    printf("  -f, --force             overwrite existing files when dowloading\n");
/*
    printf("  -t DIR, --save-to=DIR   target directory where files will be downloaded\n");
    printf("  -v, --verbose           show info messages\n");
    printf("                             if passed twice will also show debug messages\n");
    printf("\n");
    printf("  -h, --help              show this message\n");
    printf("      --version           show version information \n");
*/
    printf("\n");
}

int main(int argc, char **argv) {

    int ret = 0;
    int oper_mask = 0, opt_mask = 0;

    ret = parseargs(argc, argv, &oper_mask, &opt_mask);

    if (oper_mask & OPER_DOWNLOAD) { /* 8 */
        while (pkg_list != NULL) {
            struct aurpkg *found = aur_pkg_info((char*)pkg_list->data, &opt_mask);
            if (found != NULL) {
                char pkgURL[256];
                snprintf(pkgURL, 256, PKG_URL, found->Name, found->Name);
                get_taurball(pkgURL, NULL, &opt_mask);
            }
            free(found);
            llist_remove_node(&pkg_list);
        }
    } else if (oper_mask & OPER_INFO) { /* 4 */
        while (pkg_list != NULL) {
            struct aurpkg *found = aur_pkg_info((char*)pkg_list->data, &opt_mask);
            if (found != NULL) print_package(found, &opt_mask);
            llist_remove_node(&pkg_list);
            free(found);
        }
    } else if (oper_mask & OPER_UPDATE) { /* 2 */
        fprintf(stderr, "IOU: One Update function\n");
    } else if (oper_mask & OPER_SEARCH) { /* 1 */
        while (pkg_list != NULL) {
            aur_pkg_search((char*)pkg_list->data, &opt_mask);
            llist_remove_node(&pkg_list);
        }
    } else {
        usage();
        return 1;
    }

    return 0;
}


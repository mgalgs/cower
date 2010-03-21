/*
 *  cower.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <alpm.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <jansson.h>

#include "alpmhelper.h"
#include "aur.h"
#include "util.h"

static alpm_list_t *targets; /* Package argument list */
int oper_mask = 0, opt_mask = 0; /* Runtime Config */

static int parseargs(int argc, char **argv) {
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
        {"quiet",       no_argument,        0, 'q'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "suidcvfq", opts, &option_index))) {
        if (opt < 0) {
            break;
        }

        switch (opt) {
            case 's':
                oper_mask |= OPER_SEARCH;
                break;
            case 'u':
                oper_mask |= OPER_UPDATE;
                break;
            case 'i':
                oper_mask |= OPER_INFO;
                break;
            case 'd':
                oper_mask |= OPER_DOWNLOAD;
                break;
            case 'c':
                opt_mask |= OPT_COLOR;
                break;
            case 'v':
                opt_mask |= OPT_VERBOSE;
                break;
            case 'f':
                opt_mask |= OPT_FORCE;
                break;
            case 'q':
                opt_mask |= OPT_QUIET;
                break;
            case '?': 
                return 1;
            default: 
                return 1;
        }
    }

    /* Feed the remaining args into a linked list */
    while (optind < argc)
        targets = alpm_list_add(targets, argv[optind++]);

    return 0;
}

void usage() {
    printf("Usage: cower [options] <operation> PACKAGE [PACKAGE2..]\n\
\n\
 Operations:\n\
  -d, --download          download PACKAGE(s)\n\
  -i, --info              show info for PACKAGE(s)\n\
  -s, --search            search for PACKAGE(s)\n\
\n\
 General options:\n\
  -c, --color             use colored output\n\
  -f, --force             overwrite existing files when dowloading\n\
  -q, --quiet             show only package names in search results\n\n");
}

int main(int argc, char **argv) {

    int ret;
    ret = parseargs(argc, argv);

    /* DEBUG: Show masks and package args
    alpm_list_t *i;
    printf("oper_mask = %d\n", oper_mask);
    printf("opt_mask = %d\n", opt_mask);
    for (i = targets; i; i = alpm_list_next(i)) {
        char *pkg_arg;
        pkg_arg = alpm_list_getdata(i);
        printf("package argument: %s\n", pkg_arg);
    } END DEBUG */

    /* Order matters somewhat. Update must come before download
     * to ensure that we catch the possibility of a download flag
     * being passed along with it.
     */
    if (oper_mask & OPER_UPDATE) { /* 8 */
        alpm_quick_init();
        alpm_list_t *foreign = alpm_query_search(NULL);
        /* Do something with the list */
        alpm_list_t *i;
        for (i = foreign; i; i = alpm_list_next(i)) {
            pmpkg_t *pmpkg = alpm_list_getdata(i);
            printf("Foreign pkg: %s\n", alpm_pkg_get_name(pmpkg));
            /* TODO: Finish me */
        }
        if (oper_mask & OPER_DOWNLOAD)
            printf("I'll even download your updates too. I promise!\n");
        alpm_list_free(foreign);
    } else if (oper_mask & OPER_DOWNLOAD) { /* 4 */
        /* Check alpm for the existance of this package. If it's not in the 
         * sync, do an info query on the package in the AUR. Does it exist?
         * If yes, pass it to get_taurball.
         */
         alpm_list_t *i;
         for (i = targets; i; i = alpm_list_next(i)) {
            /* TODO: Call to pacman */
            json_t *infojson = aur_rpc_query(AUR_RPC_QUERY_TYPE_INFO,
                alpm_list_getdata(i));
            if (infojson) {
                aur_get_tarball(infojson, NULL);
                json_decref(infojson);
            }
         }
    } else if (oper_mask & OPER_INFO) { /* 2 */
        alpm_list_t *i;
        for (i = targets; i; i = alpm_list_next(i)) {
            json_t *search = 
                aur_rpc_query(AUR_RPC_QUERY_TYPE_INFO, alpm_list_getdata(i));

            if (search) {
                print_pkg_info(search);
                json_decref(search);
            }
        }
    } else if (oper_mask & OPER_SEARCH) { /* 1 */
        /* TODO: Aggregate all searches, sort, and print at once */
        alpm_list_t *i;
        for (i = targets; i; i = alpm_list_next(i)) {
            json_t *search = 
                aur_rpc_query(AUR_RPC_QUERY_TYPE_SEARCH, alpm_list_getdata(i));

            if (search) {
                print_pkg_search(search);
            }
            json_decref(search);
        }
    } else {
        usage();
        ret = 1;
    }

    /* Compulsory cleanup */
    curl_global_cleanup();
    alpm_list_free(targets);
    alpm_release();

    return ret;
}


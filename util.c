/*
 *  util.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <jansson.h>

#include "aur.h"
#include "util.h"

extern int opt_mask;

static char *pkg_category[] = { NULL, "None", "daemons", "devel",
                              "editors", "emulators", "games", "gnome",
                              "i18n", "kde", "lib", "modules",
                              "multimedia", "network", "office", "science",
                              "system", "x11", "xfce", "kernels" };


/* TODO: Figure out va macros */
int cfprint(int fd, const char* input, int color) {
    if (fd == 1 || !fd) {
        return printf("\033[1;3%dm%s\033[1;m", color, input);
    } else if (fd == 2) {
        return fprintf(stderr, "\033[1;3%dm%s\033[1;m", color, input);
    } else
        return 0;
}

void print_pkg_info(json_t *pkg) {
    char aurpage[128];
    json_t *pkginfo;
    const char *id, *name, *ver, *url, *cat, *license, *votes, *ood, *desc;

    pkginfo = json_object_get(pkg, "results");

    /* Declare pointers to json data to make my life easier */
    id      = json_string_value(json_object_get(pkginfo, "ID"));
    name    = json_string_value(json_object_get(pkginfo, "Name"));
    ver     = json_string_value(json_object_get(pkginfo, "Version"));
    url     = json_string_value(json_object_get(pkginfo, "URL"));
    cat     = json_string_value(json_object_get(pkginfo, "CategoryID"));
    license = json_string_value(json_object_get(pkginfo, "License"));
    votes   = json_string_value(json_object_get(pkginfo, "NumVotes"));
    ood     = json_string_value(json_object_get(pkginfo, "OutOfDate"));
    desc    = json_string_value(json_object_get(pkginfo, "Description"));

    /* Print it all pretty like */
    printf("Repository      : ");
    opt_mask & OPT_COLOR ? cfprint(1, "aur", MAGENTA) : printf("aur");
    putchar('\n');

    printf("Name:           : ");
    opt_mask & OPT_COLOR ? cfprint(1, name, WHITE) : printf(name);
    putchar('\n');

    printf("Version         : ");
    opt_mask & OPT_COLOR ?
        strcmp(ood, "0") ? cfprint(1, ver, RED) : cfprint(1, ver, GREEN) : printf(ver);
    putchar('\n');

    printf("URL             : ");
    opt_mask & OPT_COLOR ?  cfprint(1, url, CYAN) : printf(url);
    putchar('\n');

    snprintf(aurpage, 128, AUR_PKG_URL_FORMAT, id);
    printf("AUR Page        : ");
        if (opt_mask & OPT_COLOR) {
            cfprint(1, AUR_PKG_URL_FORMAT, CYAN);
            cfprint(1, id, CYAN);
        } else {
            printf("%s", AUR_PKG_URL_FORMAT);
            printf("%s", id);
        }
    putchar('\n');

    printf("Category        : %s\n", pkg_category[atoi(cat)]);

    printf("License         : %s\n", license);

    printf("Number of Votes : %s\n", votes);

    printf("Out Of Date     : ");
    opt_mask & OPT_COLOR ?
        strcmp(ood, "0") ?
            cfprint(1, "Yes", RED) : cfprint(1, "No", GREEN) :
            printf("%s", strcmp(ood, "0") ?  "Yes" : "No");
    putchar('\n');

    printf("Description     : %s\n\n", desc);

}

void print_pkg_search(json_t *search) {
    json_t *pkg_array, *pkg;
    unsigned int i;
    const char *name, *ver, *desc, *ood;

    pkg_array = json_object_get(search, "results");

    for (i = 0; i < json_array_size(pkg_array); i++) {
        pkg = json_array_get(pkg_array, i);

        name = json_string_value(json_object_get(pkg, "Name"));
        ver = json_string_value(json_object_get(pkg, "Version"));
        desc = json_string_value(json_object_get(pkg, "Description"));
        ood = json_string_value(json_object_get(pkg, "OutOfDate"));

        /* Line 1 */
        if (! (opt_mask & OPT_QUIET))
            opt_mask & OPT_COLOR ? cfprint(1, "aur/", MAGENTA) : printf("aur/");
        opt_mask & OPT_COLOR ? cfprint(1, name, WHITE) : printf(name);
        putchar(' ');
        opt_mask & OPT_COLOR ?
            strcmp(ood, "0") ? cfprint(1, ver, RED) : cfprint(1, ver, GREEN) :
            printf("%s", ver);
        putchar('\n');

        /* Line 2 */
        if (! (opt_mask & OPT_QUIET))
            printf("    %s\n", desc);

    }
}

int file_exists(const char* fd) {
    struct stat st;
    if (! stat(fd, &st)) {
        return 1;
    } else {
        return 0;
    }
}


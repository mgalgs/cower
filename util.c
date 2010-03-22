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

/* Standard */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Non-standard */
#include <alpm.h>
#include <jansson.h>

/* Local */
#include "aur.h"
#include "util.h"

extern int opt_mask;

static char *pkg_category[] = { NULL, "None", "daemons", "devel",
                              "editors", "emulators", "games", "gnome",
                              "i18n", "kde", "lib", "modules",
                              "multimedia", "network", "office", "science",
                              "system", "x11", "xfce", "kernels" };

int cfprintf(FILE *fd, int color, const char* arg1, ...) {

    va_list ap;
    const char *s;
    int chars_written = 0;

    va_start(ap, arg1); /* start va */

    chars_written += fprintf(fd, "\033[1;3%dm", color);
    for (s = arg1; s; s = va_arg(ap, const char*)) {
        chars_written += fprintf(fd, "%s", s);
    }
    chars_written += fprintf(fd, "\033[1;m");

    va_end(ap); /* end va */

    return chars_written;
}

void print_pkg_info(json_t *pkg) {

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
    opt_mask & OPT_COLOR ? cfprintf(stdout, MAGENTA, "aur", NULL) : printf("aur");
    putchar('\n');

    printf("Name:           : ");
    opt_mask & OPT_COLOR ? cfprintf(stdout, WHITE, name, NULL) : printf(name);
    putchar('\n');

    printf("Version         : ");
    opt_mask & OPT_COLOR ?
        cfprintf(stdout, strcmp(ood, "0") ? RED : GREEN, ver, NULL) : printf(ver);
    putchar('\n');

    printf("URL             : ");
    opt_mask & OPT_COLOR ?  cfprintf(stdout, CYAN, url, NULL) : printf(url);
    putchar('\n');

    printf("AUR Page        : ");
    opt_mask & OPT_COLOR ?
        cfprintf(stdout, CYAN, AUR_PKG_URL_FORMAT, id, NULL) :
        printf("%s%s", AUR_PKG_URL_FORMAT, id);

    putchar('\n');

    printf("Category        : %s\n", pkg_category[atoi(cat)]);

    printf("License         : %s\n", license);

    printf("Number of Votes : %s\n", votes);

    printf("Out Of Date     : ");
    opt_mask & OPT_COLOR ?
        strcmp(ood, "0") ?
            cfprintf(stdout, RED, "Yes", NULL) : cfprintf(stdout, GREEN, "No", NULL) :
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
            opt_mask & OPT_COLOR ? 
                cfprintf(stdout, MAGENTA, "aur/", NULL) : printf("aur/");
        opt_mask & OPT_COLOR ? cfprintf(stdout, WHITE, name, NULL) : printf(name);
        putchar(' ');
        opt_mask & OPT_COLOR ?
            cfprintf(stdout, strcmp(ood, "0") ? RED : GREEN, ver, NULL) :
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


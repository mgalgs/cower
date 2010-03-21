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


/* TODO: Figure out va macros and allow access to stderr */
int cprint(const char* input, int color) {
    return printf("\033[1;3%dm%s\033[1;m", color, input);
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
    opt_mask & OPT_COLOR ? cprint("aur", MAGENTA) : printf("aur");
    putchar('\n');

    printf("Name:           : ");
    opt_mask & OPT_COLOR ? cprint(name, WHITE) : printf(name);
    putchar('\n');

    printf("Version         : ");
    opt_mask & OPT_COLOR ?
        strcmp(ood, "0") ? cprint(ver, RED) : cprint(ver, GREEN) : printf(ver);
    putchar('\n');

    printf("URL             : ");
    opt_mask & OPT_COLOR ?  cprint(url, CYAN) : printf(url);
    putchar('\n');

    snprintf(aurpage, 128, AUR_PKG_URL_FORMAT, id);
    printf("AUR Page        : ");
        if (opt_mask & OPT_COLOR) {
            cprint(AUR_PKG_URL_FORMAT, CYAN);
            cprint(id, CYAN);
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
            cprint("Yes", RED) : cprint("No", GREEN) :
            printf("%s", strcmp(ood, "0") ?  "Yes" : "No");
    putchar('\n');

    printf("Description     : %s\n\n", desc);

}

int file_exists(const char* fd) {
    struct stat st;
    if (! stat(fd, &st)) {
        return 1;
    } else {
        return 0;
    }
}


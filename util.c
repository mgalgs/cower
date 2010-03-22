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

char *itoa(unsigned int num, int base){
     static char retbuf[33];
     char *p;

     if(base < 2 || base > 16)
         return NULL;

     p = &retbuf[sizeof(retbuf)-1];
     *p = '\0';

     do {
         *--p = "0123456789abcdef"[num % base];
         num /= base;
     } while(num != 0);

     return p;
}

/* Colorized printing with flexible output
 *
 * Limitation: only fixed width fields
 * Supports %d, %c, %s
 * Pass a number to %[ in fmt to turn on color
 * Use %] to turn off color
 *
 * Returns: numbers of chars written (including term escape sequences)
 */
int cfprintf(FILE *fd, const char* fmt, ...) {
    va_list ap;
    const char *p;
    int count = 0;

    int i; char *s; /* va_arg containers */

    va_start(ap, fmt);

    for (p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            fputc(*p, fd); count++;
            continue;
        }

        switch (*++p) {
        case 'c':
            i = va_arg(ap, int);
            fputc(i, fd); count++;
            break;
        case 's':
            s = va_arg(ap, char*);
            count += fputs(s, fd);
            break;
        case 'd':
            i = va_arg(ap, int);
            if (i < 0) {
                i = -i;
                fputc('-', fd);
            }
            count += fputs(itoa(i, 10), fd);
            break;
        case '!': /* color on */
            count += fputs(C_ON, fd);
            count += fputs(itoa(va_arg(ap, int), 10), fd);
            fputc('m', fd); count++;
            break;
        case '@': /* color off */
            count += fputs(C_OFF, fd);
            break;
        case '%':
            fputc('%', fd); count++;
            break;
        }
    }
    va_end(ap);

    return count;
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
    opt_mask & OPT_COLOR ?
        cfprintf(stdout, "%[aur%]\n", MAGENTA) : printf("aur\n");

    printf("Name:           : ");
    opt_mask & OPT_COLOR ?
        cfprintf(stdout, "%[%s%]\n", WHITE, name) : printf("%s\n", name);

    printf("Version         : ");
    opt_mask & OPT_COLOR ?
        cfprintf(stdout, "%[%s%]\n", strcmp(ood, "0") ? RED : GREEN, ver) :
        printf("%s\n", ver);

    printf("URL             : ");
    opt_mask & OPT_COLOR ?
        cfprintf(stdout, "%[%s%]\n", CYAN, url, NULL) : printf("%s\n", url);

    printf("AUR Page        : ");
    opt_mask & OPT_COLOR ?
        cfprintf(stdout, "%[%s%s%]\n", CYAN, AUR_PKG_URL_FORMAT, id) :
        printf("%s%s\n", AUR_PKG_URL_FORMAT, id);

    printf("Category        : %s\n", pkg_category[atoi(cat)]);

    printf("License         : %s\n", license);

    printf("Number of Votes : %s\n", votes);

    printf("Out Of Date     : ");
    opt_mask & OPT_COLOR ?
        strcmp(ood, "0") ?
            cfprintf(stdout, "%[Yes%]\n", RED) : 
                cfprintf(stdout, "%[No%]\n", GREEN) :
            printf("%s\n", strcmp(ood, "0") ?  "Yes" : "No");

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
                cfprintf(stdout, "%[aur/%]", MAGENTA) : printf("aur/");
        opt_mask & OPT_COLOR ? 
            cfprintf(stdout, "%[%s%]", WHITE, name) : printf(name);
        putchar(' ');
        opt_mask & OPT_COLOR ?
            cfprintf(stdout, "%[%s%]", strcmp(ood, "0") ? RED : GREEN, ver) :
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


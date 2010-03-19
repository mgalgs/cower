/*
 *  package.c
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

#include <jansson.h>

#include "package.h"
#include "util.h"

#define AURPAGE_FORMAT "http://aur.archlinux.org/packages.php?ID=%d"

static char *pkg_category[] = { NULL, "None", "daemons", "devel",
                              "editors", "emulators", "games", "gnome",
                              "i18n", "kde", "lib", "modules",
                              "multimedia", "network", "office", "science",
                              "system", "x11", "xfce", "kernels" };

void get_pkg_details(json_t *package, struct aurpkg **aur_pkg) {
    (*aur_pkg)->ID =
        atoi(json_string_value(json_object_get(package, "ID")));

    (*aur_pkg)->NumVotes =
        atoi(json_string_value(json_object_get(package, "NumVotes")));

    (*aur_pkg)->OutOfDate =
        atoi(json_string_value(json_object_get(package, "OutOfDate")));

    (*aur_pkg)->CategoryID =
        atoi(json_string_value(json_object_get(package, "CategoryID")));

    (*aur_pkg)->LocationID =
        atoi(json_string_value(json_object_get(package, "LocationID")));

    (*aur_pkg)->Name = strdup(json_string_value(json_object_get(package, "Name")));
    (*aur_pkg)->Version = strdup(json_string_value(json_object_get(package, "Version")));
    (*aur_pkg)->Description = strdup(json_string_value(json_object_get(package, "Description")));
    (*aur_pkg)->URL = strdup(json_string_value(json_object_get(package, "URL")));
    (*aur_pkg)->URLPath = strdup(json_string_value(json_object_get(package, "URLPath")));
    (*aur_pkg)->License = strdup(json_string_value(json_object_get(package, "License")));
}

void print_package(struct aurpkg *pkg, int *opt_mask) {
    char buffer[256];
    char aurpage[256];

    printf("Repository      : %s\n", *opt_mask & 1 ?
        colorize("aur", MAGENTA, buffer) : "aur");

    printf("Name            : %s\n", *opt_mask & 1 ?
        colorize(pkg->Name, WHITE, buffer) : pkg->Name);

    printf("Version         : %s\n", *opt_mask & 1 ?
        pkg->OutOfDate ?
            colorize(pkg->Version, RED, buffer) :
                colorize(pkg->Version, GREEN, buffer) : 
            pkg->Version);

    printf("URL             : %s\n", *opt_mask & 1 ?
                colorize(pkg->URL, CYAN, buffer) : pkg->URL);

    sprintf(aurpage, AURPAGE_FORMAT, pkg->ID);
    printf("AUR Page        : %s\n", *opt_mask & 1 ?
        colorize(aurpage, CYAN, buffer) : aurpage);

    printf("Category        : %s\n", pkg_category[pkg->CategoryID]);
    printf("License         : %s\n", pkg->License);
    printf("Number of Votes : %d\n", pkg->NumVotes);
    printf("Out Of Date     : %s\n", *opt_mask & 1 ?
        pkg->OutOfDate ? colorize("Yes", RED, buffer) : colorize("No", GREEN, buffer) :
        pkg->OutOfDate ? "Yes" : "No" );

    printf("Description     : %s\n\n", pkg->Description);
}


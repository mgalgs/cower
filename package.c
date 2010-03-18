#include <stdio.h>

#include <jansson.h>
#include "package.h"

static char *pkg_category[] = { NULL, "None", "daemons", "devel",
                              "editors", "emulators", "games", "gnome",
                              "i18n", "kde", "lib", "modules",
                              "multimedia", "network", "office", "science",
                              "system", "x11", "xfce", "kernels" };

void get_pkg_details(json_t *package, struct aurpkg **aur_pkg) {
    (*aur_pkg)->ID = atoi(json_string_value(json_object_get(package, "ID")));
    (*aur_pkg)->NumVotes = atoi(json_string_value(json_object_get(package, "NumVotes")));
    (*aur_pkg)->OutOfDate = atoi(json_string_value(json_object_get(package, "OutOfDate")));
    (*aur_pkg)->CategoryID = atoi(json_string_value(json_object_get(package, "CategoryID")));
    (*aur_pkg)->LocationID = atoi(json_string_value(json_object_get(package, "LocationID")));

    (*aur_pkg)->Name = json_string_value(json_object_get(package, "Name"));
    (*aur_pkg)->Version = json_string_value(json_object_get(package, "Version"));
    (*aur_pkg)->Description = json_string_value(json_object_get(package, "Description"));
    (*aur_pkg)->URL = json_string_value(json_object_get(package, "URL"));
    (*aur_pkg)->URLPath = json_string_value(json_object_get(package, "URLPath"));
    (*aur_pkg)->License = json_string_value(json_object_get(package, "License"));
}

void print_package(struct aurpkg *pkg) {

    printf("Repository      : aur\n");
    printf("Name            : %s\n", pkg->Name);
    printf("Version         : %s\n", pkg->Version);
    printf("URL             : %s\n", pkg->URL);
    printf("AUR Page        : http://aur.archlinux.org/packages.php?ID=%d\n", pkg->ID);
    printf("Category        : %s\n", pkg_category[pkg->CategoryID]);
    printf("License         : %s\n", pkg->License);
    printf("Number of Votes : %d\n", pkg->NumVotes);

    //printf("LocationID      : %d\n", pkg->LocationID);
    //printf("URLPath         : %s\n", pkg->URLPath);
    printf("OutOfDate       : %s\n", pkg->OutOfDate == 1 ? "Yes" : "No");
    printf("Description     : %s\n\n", pkg->Description);
}

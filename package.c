#include <stdio.h>
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
    (*aur_pkg)->ID = atoi(json_string_value(json_object_get(package, "ID")));
    (*aur_pkg)->NumVotes = atoi(json_string_value(json_object_get(package, "NumVotes")));
    (*aur_pkg)->OutOfDate = atoi(json_string_value(json_object_get(package, "OutOfDate")));
    (*aur_pkg)->CategoryID = atoi(json_string_value(json_object_get(package, "CategoryID")));
    (*aur_pkg)->LocationID = atoi(json_string_value(json_object_get(package, "LocationID")));

    (*aur_pkg)->Name = strdup(json_string_value(json_object_get(package, "Name")));
    (*aur_pkg)->Version = strdup(json_string_value(json_object_get(package, "Version")));
    (*aur_pkg)->Description = strdup(json_string_value(json_object_get(package, "Description")));
    (*aur_pkg)->URL = strdup(json_string_value(json_object_get(package, "URL")));
    (*aur_pkg)->URLPath = strdup(json_string_value(json_object_get(package, "URLPath")));
    (*aur_pkg)->License = strdup(json_string_value(json_object_get(package, "License")));
}

void print_package(struct aurpkg *pkg, int *opt_mask) {
    char buffer[256];

    printf("Repository      : %s\n", *opt_mask & 1 ? colorize("aur", MAGENTA, buffer) : "aur");
    printf("Name            : %s\n", *opt_mask & 1 ? colorize(pkg->Name, WHITE, buffer) : pkg->Name);
    printf("Version         : %s\n", pkg->OutOfDate == 1 ? 
                (*opt_mask & 1 ? colorize(pkg->Version, RED, buffer) : pkg->Version) : 
                (*opt_mask & 1 ? colorize(pkg->Version, GREEN, buffer) : pkg->Version));

    printf("URL             : %s\n", *opt_mask & 1 ? 
                colorize(pkg->URL, CYAN, buffer) : pkg->URL);

    char aurpage[256]; sprintf(aurpage, AURPAGE_FORMAT, pkg->ID);
    printf("AUR Page        : %s\n", *opt_mask & 1 ? colorize(aurpage, CYAN, buffer) : aurpage);
    printf("Category        : %s\n", pkg_category[pkg->CategoryID]);
    printf("License         : %s\n", pkg->License);
    printf("Number of Votes : %d\n", pkg->NumVotes);
    printf("Out Of Date     : %s\n", pkg->OutOfDate == 1 ?
                (*opt_mask & 1 ? colorize("Yes", RED, buffer) : "Yes") :
                (*opt_mask & 1 ? colorize("No", GREEN, buffer) : "No"));
    printf("Description     : %s\n\n", pkg->Description);
}


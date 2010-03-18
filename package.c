#include <stdio.h>

#include <jansson.h>
#include "package.h"

void get_pkg_details(json_t *package, struct aurpkg **aur_pkg) {
    (*aur_pkg)->ID = atoi(json_string_value(json_object_get(package, "ID")));
    (*aur_pkg)->NumVotes = atoi(json_string_value(json_object_get(package, "NumVotes")));
    (*aur_pkg)->OutOfDate = atoi(json_string_value(json_object_get(package, "OutOfDate")));
    (*aur_pkg)->CategoryID = atoi(json_string_value(json_object_get(package, "CategoryID")));
    (*aur_pkg)->LocationID = atoi(json_string_value(json_object_get(package, "LocationID")));
    (*aur_pkg)->LocationID = atoi(json_string_value(json_object_get(package, "LocationID")));

    (*aur_pkg)->Name = json_string_value(json_object_get(package, "Name"));
    (*aur_pkg)->Version = json_string_value(json_object_get(package, "Version"));
    (*aur_pkg)->Description = json_string_value(json_object_get(package, "Description"));
    (*aur_pkg)->URL = json_string_value(json_object_get(package, "URL"));
    (*aur_pkg)->URLPath = json_string_value(json_object_get(package, "URLPath"));
    (*aur_pkg)->License = json_string_value(json_object_get(package, "License"));
}

void print_package(struct aurpkg *pkg) {
    printf("ID: %d\n", pkg->ID);
    printf("Name: %s\n", pkg->Name);
    printf("Version: %s\n", pkg->Version);
    printf("CategoryID: %d\n", pkg->CategoryID);
    printf("Description: %s\n", pkg->Description);
    printf("LocationID: %d\n", pkg->LocationID);
    printf("URL: %s\n", pkg->URL);
    printf("URLPath: %s\n", pkg->URLPath);
    printf("License: %s\n", pkg->License);
    printf("NumVotes: %d\n", pkg->NumVotes);
    printf("OutOfDate: %s\n\n", pkg->OutOfDate == 1 ? "Yes" : "No");
}

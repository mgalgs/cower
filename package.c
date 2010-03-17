#include <stdio.h>

#include "package.h"

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
    printf("OutOfDate: %s\n", pkg->OutOfDate == 1 ? "Yes" : "No");
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>

#include "aur.h"
#include "package.h"
#include "curlhelper.h"


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

struct aurpkg *aur_pkg_info(char* req) {
    char *text;
    char url[URL_SIZE];

    json_t *root;
    json_error_t error;
    json_t *package;

    snprintf(url, URL_SIZE, URL_FORMAT, "info", req);

    text = request(url);
    if(!text)
        return NULL;

    root = json_loads(text, &error);
    free(text);

    if(!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return NULL;
    }

    package = json_object_get(root, "results");

    /* No result found */
    if(!json_is_object(package)) {
        return NULL;
    }

    struct aurpkg *pkg = malloc(sizeof(struct aurpkg));
    get_pkg_details(package, &pkg);

    //free(pkg);
    //json_decref(root);

    return pkg;
}

static struct json_t *aur_pkg_search( ) {
    return NULL;
}

void usage(void) {
    fprintf(stderr, "usage: cower TYPE PACKAGE\n\n");
}

int main(int argc, char **argv) {

    if(argc != 3) {
        usage();
        return 1;
    }

    if (! strcmp(argv[1], "info")) {
        struct aurpkg *result = aur_pkg_info(argv[2]);
        if (result != NULL) {
            print_package(result);
        } else {
            fprintf(stderr, "Error: Package %s not found\n", argv[2]);
        }
        free(result);
    } else if (! strcmp(argv[1], "search")) {
        fprintf(stderr, "Lol search not implemented\n");
    } else {
        usage();
    }

    return 0;
}


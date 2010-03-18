/* Standard */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Non-standard */
#include <jansson.h>

/* Local */
#include "aur.h"
#include "curlhelper.h"
#include "package.h"

struct json_t *aur_pkg_search(char* req, int* opt_mask) {
    char *text;
    char url[URL_SIZE];

    json_t *root;
    json_error_t error;
    json_t *search_res;

    /* Format URL to pass to curl */
    snprintf(url, URL_SIZE, URL_FORMAT, "search", req);

    text = request(url);
    if(!text)
        return NULL;

    /* Fetch JSON */
    root = json_loads(text, &error);
    free(text);

    /* Check for error */
    if(!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return NULL;
    }

    /* Grab results object in JSON */
    search_res = json_object_get(root, "results");

    /* Valid results? */
    if(!json_is_array(search_res)) {
        printf("not an array\n");
        return NULL;
    }

    print_search_results(search_res);
    json_decref(root);

    return NULL; //search_res;
}

void print_search_results(json_t* search_res) {
    unsigned int i;

    json_t *pkg;
    const char *name, *version, *desc;
    int ood;
    for(i = 0; i < json_array_size(search_res); i++) {
        pkg = json_array_get(search_res, i);

        name = json_string_value(json_object_get(pkg, "Name"));
        version = json_string_value(json_object_get(pkg, "Version"));
        desc = json_string_value(json_object_get(pkg, "Description"));
        ood = atoi(json_string_value(json_object_get(pkg, "OutOfDate")));

        printf("aur/%s %s (%d)\n    %s\n", name, version, ood, desc);
    }
}

struct aurpkg *aur_pkg_info(char* req, int* opt_mask) {
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
        fprintf(stderr, "Error: no result found for %s\n\n", req);
        return NULL;
    }

    struct aurpkg *pkg = malloc(sizeof(struct aurpkg));
    get_pkg_details(package, &pkg);

    json_decref(root);

    return pkg;
}


/*
 *  aur.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <curl/curl.h>

/* Non-standard */
#include <jansson.h>

/* Local */
#include "util.h"
#include "aur.h"
#include "curlhelper.h"
#include "package.h"

struct json_t *aur_pkg_search(char* req, int* opt_mask) {
    char *text;
    char url[URL_SIZE];
    char buffer[32];

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
        fprintf(stderr, "%s no results for \"%s\"\n", 
            *opt_mask & OPT_COLOR ? colorize("error:", RED, buffer) : "error:",
            req);
        return NULL;
    }

    print_search_results(search_res, opt_mask);
    json_decref(root);

    return NULL;
}

void print_search_results(json_t* search_res, int* opt_mask) {
    unsigned int i;

    json_t *pkg;
    const char *name, *version, *desc;
    int ood;
    char buffer[256];

    for(i = 0; i < json_array_size(search_res); i++) {
        pkg = json_array_get(search_res, i);

        name = json_string_value(json_object_get(pkg, "Name"));
        version = json_string_value(json_object_get(pkg, "Version"));
        desc = json_string_value(json_object_get(pkg, "Description"));
        ood = atoi(json_string_value(json_object_get(pkg, "OutOfDate")));

        printf("%s", *opt_mask & OPT_COLOR ? colorize("aur/", MAGENTA, buffer) : "aur/");
        printf("%s ", *opt_mask & OPT_COLOR ? colorize(name, WHITE, buffer) : name);
        printf("%s\n", *opt_mask & OPT_COLOR ?
            (ood ? colorize(version, RED, buffer) : colorize(version, GREEN, buffer)) :
            version);
        printf("    %s\n", desc);
    }
}

struct aurpkg *aur_pkg_info(char* req, int* opt_mask) {
    char *text;
    char url[URL_SIZE];

    json_t *root;
    json_error_t error;
    json_t *package;
    char buffer[32];

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
        fprintf(stderr, "%s package \"%s\" not found\n", 
            *opt_mask & OPT_COLOR ? colorize("error:", RED, buffer) : "error:",
            req);
        return NULL;
    }

    struct aurpkg *pkg = malloc(sizeof(struct aurpkg));
    get_pkg_details(package, &pkg);

    json_decref(root);

    return pkg;
}

int get_taurball(const char *url, char *target_dir, int *opt_mask) {
    CURL *curl;
    FILE *fd;
    char *dir, *filename, *fullpath, buffer[256];
    int result = 0;

    if (target_dir == NULL) { /* Use pwd */
        dir = getcwd(NULL, 0);
    } else { /* TODO: Implement the option for this */
        dir = target_dir;
    }

    filename = strrchr(url, '/') + 1;

    /* Construct the full path */
    fullpath = calloc(strlen(dir) + strlen(filename), 1);
    fullpath = strncat(fullpath, dir, strlen(dir));
    fullpath = strncat(fullpath, "/", 1);
    fullpath = strncat(fullpath, filename, strlen(filename));

    if (file_exists(filename) && ! (*opt_mask & OPT_FORCE)) {
        fprintf(stderr, "Error: %s/%s already exists.\nUse -f to force this operation.\n",
            dir, filename);
        result = 1;
    } else {
        fd = fopen(filename, "w");
        if (fd != NULL) {
            curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
            result = curl_easy_perform(curl);

            curl_easy_cleanup(curl);
            curl_global_cleanup();

            filename[strlen(filename) - 7] = '\0'; /* hackity hack basename */
            printf("%.*s", filename, *opt_mask & OPT_COLOR ? colorize(filename, WHITE, buffer) : filename);
            printf(" downloaded to ");
            printf("%s\n",
                *opt_mask & OPT_COLOR ? colorize(dir, GREEN, buffer) : dir);

            fclose(fd);

            pid_t pid; pid = fork();
            if (pid == 0) { /* Child process */
                result = execlp("bsdtar", "bsdtar", "-xf", fullpath, NULL);
            } else { /* Back in the parent, waiting for child to finish */
                while (! waitpid(pid, NULL, WNOHANG));
                if (! result) { /* If we get here, bsdtar finished successfully */
                    unlink(fullpath);
                }
            }
        } else {
            fprintf(stderr, "Error writing to path: %s\n", dir);
            result = 1;
        }
    }

    free(fullpath);
    free(dir);
    return result;
}


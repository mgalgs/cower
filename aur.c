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
#include <sys/wait.h>
#include <unistd.h>

/* Non-standard */
#include <curl/curl.h>
#include <jansson.h>

/* Local */
#include "aur.h"
#include "json.h"
#include "util.h"

extern int opt_mask;

json_t *aur_rpc_query(int type, char* arg) {
    char *text;
    char url[AUR_RPC_URL_SIZE];
    char buffer[32];

    json_t *root;
    json_error_t error;
    json_t *return_type;

    /* Format URL to pass to curl */
    snprintf(url, AUR_RPC_URL_SIZE, AUR_RPC_URL,
        type == AUR_RPC_QUERY_TYPE_INFO ? "info" : "search",
        arg);

    text = curl_get_json(url);
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

    /* Check return type in JSON */
    return_type = json_object_get(root, "type");
    if (! strcmp(json_string_value(return_type), "error")) {
        fprintf(stderr, "%s no results for \"%s\"\n", 
            opt_mask & OPT_COLOR ? colorize("error:", RED, buffer) : "error:",
            arg);
        json_decref(root);
        return NULL;
    }

    return root; /* This needs to be freed in the calling function */
}

int aur_get_tarball(json_t *pkginfo, char *target_dir) {
    CURL *curl;
    FILE *fd;
    char *dir, *filename, *fullpath, url[128], buffer[256];
    int result = 0;

    if (target_dir == NULL) { /* Use pwd */
        dir = getcwd(NULL, 0);
    } else { /* TODO: Implement the option for this */
        dir = target_dir;
    }

    /* Build URL. Need this to get the taurball, durp */
    snprintf(url, 128, AUR_PKG_URL,
        json_string_value(json_object_get(pkginfo, "URLPath")));

    /* Pointer to the 'basename' of the URL Path */
    filename = strrchr(url, '/');

    /* ...and the full path to the taurball! */
    fullpath = calloc(strlen(dir) + strlen(filename), sizeof(char));
    fullpath = strncat(fullpath, dir, strlen(dir));
    fullpath = strncat(fullpath, filename, strlen(filename));

    if (file_exists(fullpath) && ! (opt_mask & OPT_FORCE)) {
        fprintf(stderr, "%s %s already exists.\nUse -f to force this operation.\n", 
            opt_mask & OPT_COLOR ? colorize("error:", RED, buffer) : "error:",
            fullpath);
        result = 1;
    } else {
        fullpath[strlen(fullpath)] = '.'; /* Unmask file extension */
        fd = fopen(filename, "w");
        if (fd != NULL) {
            curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
            result = curl_easy_perform(curl);

            curl_easy_cleanup(curl);
            curl_global_cleanup();

            filename[strlen(filename) - 7] = '\0'; /* hackity hack basename */
            printf("%s", opt_mask & OPT_COLOR ? colorize(filename, WHITE, buffer) : filename);
            printf(" downloaded to ");
            printf("%s\n",
                opt_mask & OPT_COLOR ? colorize(dir, GREEN, buffer) : dir);

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


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
#include <alpm.h>
#include <curl/curl.h>
#include <jansson.h>

/* Local */
#include "aur.h"
#include "json.h"
#include "util.h"

extern int opt_mask;
extern int oper_mask;

void aur_find_updates(alpm_list_t *foreign) {

    alpm_list_t *i;

    /* Iterate over foreign packages */
    for (i = foreign; i; i = alpm_list_next(i)) {
        pmpkg_t *pmpkg = alpm_list_getdata(i);

        /* Do I exist in the AUR? */
        json_t *infojson = aur_rpc_query(AUR_RPC_QUERY_TYPE_INFO,
            alpm_pkg_get_name(pmpkg));

        if (infojson != NULL) { /* Yes, I do exist! */
            json_t *pkg;
            const char *aur_ver, *local_ver;

            pkg = json_object_get(infojson, "results");
            aur_ver = json_string_value(json_object_get(pkg, "Version"));
            local_ver = alpm_pkg_get_version(pmpkg);

            /* Version check */
            if (alpm_pkg_vercmp(aur_ver, local_ver) > 0) {
                if (oper_mask & OPER_DOWNLOAD) { /* -d found with -u */
                        aur_get_tarball(infojson, NULL);
                } else {
                    if (opt_mask & OPT_COLOR) {
                        cfprint(1, alpm_pkg_get_name(pmpkg), WHITE);
                        if (! (opt_mask & OPT_QUIET)) {
                            putchar(' ');
                            cfprint(1, local_ver, GREEN);
                            printf(" -> ");
                            cfprint(1, aur_ver, GREEN);
                        }
                        putchar('\n');
                    } else {
                        if (! (opt_mask & OPT_QUIET)) {
                            printf("%s %s -> %s\n", alpm_pkg_get_name(pmpkg),
                                local_ver, aur_ver);
                        } else {
                            printf("%s\n", alpm_pkg_get_name(pmpkg));
                        }
                    }
                }
            }
            json_decref(infojson);
        }
    }
}

int aur_get_tarball(json_t *root, char *target_dir) {

    CURL *curl;
    FILE *fd;
    const char *dir, *filename, *pkgname;
    char *fullpath, url[128];
    int result = 0;
    json_t *pkginfo;

    if (target_dir == NULL) { /* Use pwd */
        dir = getcwd(NULL, 0);
    } else { /* TODO: Implement the option for this */
        dir = target_dir;
    }

    /* Point to the juicy bits of the JSON */
    pkginfo = json_object_get(root, "results");

    /* Build URL. Need this to get the taurball, durp */
    snprintf(url, 128, AUR_PKG_URL,
        json_string_value(json_object_get(pkginfo, "URLPath")));

    /* Point to the package name */
    pkgname = json_string_value(json_object_get(pkginfo, "Name"));

    /* Pointer to the 'basename' of the URL Path */
    filename = strrchr(url, '/');

    /* ...and the full path to the taurball! */
    fullpath = calloc(strlen(dir) + strlen(filename), sizeof(char) + 1);
    fullpath = strncat(fullpath, dir, strlen(dir));
    fullpath = strncat(fullpath, filename, strlen(filename));

    filename++; /* Get rid of the leading slash */

    if (file_exists(fullpath) && ! (opt_mask & OPT_FORCE)) {
        opt_mask & OPT_COLOR ? cfprint(2, "error:", RED) :
            fprintf(stderr, "error:");
        fprintf(stderr, " %s already exists.\nUse -f to force this operation.\n", 
            fullpath);
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

            opt_mask & OPT_COLOR ? cfprint(1, pkgname, WHITE) : printf(pkgname);
            printf(" downloaded to ");
            opt_mask & OPT_COLOR ? cfprint(1, dir, GREEN) : printf(dir);
            putchar('\n');

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
    free((void*)dir);

    return result;
}

json_t *aur_rpc_query(int type, const char* arg) {

    char *text;
    char url[AUR_RPC_URL_SIZE];
    json_t *root, *return_type;
    json_error_t error;

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
        /*
        opt_mask & OPT_COLOR ? cfprint(2, "error:", RED) :
            fprintf(stderr, "error:"),
        fprintf(stderr, " no results for \"%s\"\n", arg);
        */
        json_decref(root);
        return NULL;
    }

    return root; /* This needs to be freed in the calling function */
}


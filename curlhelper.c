#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <jansson.h>

#include "curlhelper.h"
#include "util.h"
#include "aur.h"

int newline_offset(const char *text) {
    const char *newline = strchr(text, '\n');
    if(!newline) {
        return strlen(text);
    } else {
        return (int)(newline - text);
    }
}

size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
    struct write_result *result = (struct write_result *)stream;

    if(result->pos + size * nmemb >= BUFFER_SIZE - 1) {
        fprintf(stderr, "error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

int get_taurball(const char *url, char *target_dir, int *opt_mask) {
    CURL *curl;
    FILE *fd;
    char *dir;
    char *filename;
    char buffer[256];

    //if (target_dir == NULL) { /* Use pwd */
        dir = get_current_dir_name();
    //}

    filename = strrchr(url, '/') + 1;
    //printf("filename = %s\n", filename);

    if (file_exists(filename) && ! (*opt_mask & OPT_FORCE)) {
        fprintf(stderr, "Error: %s/%s already exists.\nUse -f to force this operation.\n",
            dir, filename);
    } else {
        fd = fopen(filename, "w");
        if (fd != NULL) {
            curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
            int result = curl_easy_perform(curl);

            printf("%s downloaded to ",
                *opt_mask & 1 ? colorize(filename, WHITE, buffer) : filename);
            printf("%s\n",
                *opt_mask & 1 ? colorize(dir, GREEN, buffer) : dir);

            fclose(fd);
        } else {
            fprintf(stderr, "Error writing to path: %s\n", dir);
        }

        free(dir);
        return 0;
    }
}

char *request(const char *url) {
    CURL *curl;
    CURLcode status;
    char *data;
    long code;

    curl = curl_easy_init();
    data = malloc(BUFFER_SIZE);
    if(!curl || !data) return NULL;

    struct write_result write_result = {
        .data = data,
        .pos = 0
    };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    status = curl_easy_perform(curl);
    if(status != 0) {
        fprintf(stderr, "error: unable to request data from %s:\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code != 200) {
        fprintf(stderr, "error: server responded with code %ld\n", code);
        return NULL;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;
}


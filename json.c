/*
 *  json.c
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
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Non-standard */
#include <alpm.h>
#include <jansson.h>

/* Local */
#include "aur.h"
#include "json.h"

extern CURL *curl;

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
    struct write_result *result = (struct write_result *)stream;

    if(result->pos + size * nmemb >= JSON_BUFFER_SIZE - 1) {
        fprintf(stderr, "curl error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

char *curl_get_json(const char *url) {
    CURLcode status;
    char *data;
    long code;

    data = malloc(JSON_BUFFER_SIZE);
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
        fprintf(stderr, "curl error: unable to request data from %s\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        free(data);
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code != 200) {
        fprintf(stderr, "curl error: server responded with code %ld\n", code);
        free(data);
        return NULL;
    }

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;
}


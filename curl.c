/*
 *  curl.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "curl.h"
#include "util.h"

struct response {
  size_t size;
  char *data;
};

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
  size_t realsize = size * nmemb;
  struct response *mem = (struct response*)data;

  mem->data = realloc(mem->data, mem->size + realsize + 1);

  if (mem->data) {
    memcpy(&(mem->data[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
  }

  return realsize;
}

char *curl_textfile_get(const char *url) {
  long httpcode;
  CURLcode curlstat;

  struct response curl_data = {
    .size = 0,
    .data = NULL
  };

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curl_data);

  curlstat = curl_easy_perform(curl);
  if (curlstat != CURLE_OK) {
    fprintf(stderr, "!! curl: %s\n", curl_easy_strerror(curlstat));
    return NULL;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode != 200) {
    if (config->color)
      cfprintf(stderr, "%<::%> curl: server responded with code %l\n",
        config->colors->error, httpcode);
    else
      fprintf(stderr, "!! curl: server responded with code %ld\n", httpcode);

    free(curl_data.data);
    return NULL;
  }

  return curl_data.data;
}

int curl_local_init() {
  curl = curl_easy_init();

  if (! curl)
    return 1;

  if (config->verbose >= 3) { /* super secret verbosity level */
    printf("::DEBUG:: Initializing curl\n");
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
  }

  curl_easy_setopt(curl, CURLOPT_USERAGENT, COWER_USERAGENT);
  curl_easy_setopt(curl, CURLOPT_ENCODING, "deflate, gzip");

  return 0;
}

/* vim: set ts=2 sw=2 et: */

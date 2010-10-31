/* Copyright (c) 2010 Dave Reisner
 *
 * curl.c
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aur.h"
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

  return(realsize);
}

char *curl_textfile_get(const char *arg) {
  long httpcode;
  CURLcode curlstat;
  CURL *curl;
  char *escaped, *url;

  struct response curl_data = {
    .size = 0,
    .data = NULL
  };

  curl = curl_local_init();

  escaped = curl_easy_escape(curl, arg, strlen(arg));
  cwr_asprintf(&url, AUR_PKGBUILD_PATH, escaped, escaped);
  curl_free(escaped);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curl_data);

  curlstat = curl_easy_perform(curl);
  if (curlstat != CURLE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: %s\n", curl_easy_strerror(curlstat));
    return(NULL);
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode != 200) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: server responded with code %ld\n",
        httpcode);

    free(curl_data.data);
    return(NULL);
  }

  curl_easy_cleanup(curl);
  free(url);
  return(curl_data.data);
}

CURL *curl_local_init() {
  CURL *curl;

  if (!(curl = curl_easy_init())) {
    return(NULL);
  }

  cwr_fprintf(stdout, LOG_DEBUG, "Initializing curl handle\n");

  curl_easy_setopt(curl, CURLOPT_USERAGENT, COWER_USERAGENT);
  curl_easy_setopt(curl, CURLOPT_ENCODING, "deflate, gzip");

  return(curl);
}

/* vim: set ts=2 sw=2 et: */

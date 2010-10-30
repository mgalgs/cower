/* Copyright (c) 2010 Dave Reisner
 *
 * yajl.c
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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yajl/yajl_parse.h>

#include "aur.h"
#include "conf.h"
#include "curl.h"
#include "package.h"
#include "util.h"
#include "yajl.h"

struct yajl_parse_struct {
  alpm_list_t **pkg_list;
  struct aur_pkg_t *aurpkg;
  char curkey[32];
  int json_depth;
};

static int json_string(void *ctx, const unsigned char *data, unsigned int size) {
  struct yajl_parse_struct *parse_struct = (struct yajl_parse_struct*)ctx;
  const char *val = (const char*)data;

  if(STREQ(parse_struct->curkey, AUR_QUERY_TYPE) &&
      STR_STARTS_WITH(val, AUR_QUERY_ERROR)) {
    return(0);
  }

  if (STREQ(parse_struct->curkey, AUR_ID)) {
    parse_struct->aurpkg->id = atoi(val);
  } else if (STREQ(parse_struct->curkey, AUR_NAME)) {
    parse_struct->aurpkg->name = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_VER)) {
    parse_struct->aurpkg->ver = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_CAT)) {
    parse_struct->aurpkg->cat = atoi(val);
  } else if (STREQ(parse_struct->curkey, AUR_DESC)) {
    parse_struct->aurpkg->desc = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_URL)) {
    parse_struct->aurpkg->url = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_URLPATH)) {
    parse_struct->aurpkg->urlpath = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_LICENSE)) {
    parse_struct->aurpkg->lic = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_VOTES)) {
    parse_struct->aurpkg->votes = atoi(val);
  } else if (STREQ(parse_struct->curkey, AUR_OOD)) {
    parse_struct->aurpkg->ood = *val == '1' ? 1 : 0;
  }

  return(1);
}

static int json_map_key(void *ctx, const unsigned char *data, unsigned int size) {
  struct yajl_parse_struct *parse_struct = (struct yajl_parse_struct*)ctx;

  strncpy(parse_struct->curkey, (const char*)data, size);
  parse_struct->curkey[size] = '\0';

  return(1);
}

static int json_start_map(void *ctx) {
  struct yajl_parse_struct *parse_struct = (struct yajl_parse_struct*)ctx;

  if (parse_struct->json_depth++ >= 1) {
    parse_struct->aurpkg = aur_pkg_new();
  }

  return(1);
}


static int json_end_map(void *ctx) {
  struct yajl_parse_struct *parse_struct = (struct yajl_parse_struct*)ctx;

  if (!--(parse_struct->json_depth)) {
    aur_pkg_free(parse_struct->aurpkg);
    return(0);
  }

  *parse_struct->pkg_list = alpm_list_add_sorted(*parse_struct->pkg_list,
                                                 parse_struct->aurpkg,
                                                 aur_pkg_cmp);
  parse_struct->aurpkg = NULL;

  return(1);
}

static yajl_callbacks callbacks = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  json_string,
  json_start_map,
  json_map_key,
  json_end_map,
  NULL,
  NULL
};

static size_t curl_write_yajl(void *ptr, size_t size, size_t nmemb, void *stream) {
  yajl_handle *hand = (yajl_handle*)stream;

  size_t realsize = size * nmemb;
  yajl_parse(*hand, ptr, realsize);
  return(realsize);
}

alpm_list_t *aur_fetch_json(const char *url) {
  alpm_list_t *results;
  CURLcode curlstat;
  long httpcode;
  struct yajl_parse_struct *parse_struct;
  yajl_handle hand;

  results = NULL;

  parse_struct = malloc(sizeof *parse_struct);
  parse_struct->pkg_list = &results;
  parse_struct->aurpkg = NULL;
  parse_struct->json_depth = 0;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_yajl);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &hand);

  hand = yajl_alloc(&callbacks, NULL, NULL, (void*)parse_struct);

  curlstat = curl_easy_perform(curl);
  if (curlstat != CURLE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: %s\n", curl_easy_strerror(curlstat));
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode != 200) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: server responded with code %ld\n", httpcode); 
  }

  aur_pkg_free(parse_struct->aurpkg);
  free(parse_struct);
  yajl_parse_complete(hand);
  yajl_free(hand);

  return(results);
}

/* vim: set ts=2 sw=2 et: */

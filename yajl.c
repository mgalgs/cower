/*
 *  yajl.c
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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

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

  if(STREQ(parse_struct->curkey, AUR_QUERY_TYPE) && STR_STARTS_WITH(val, AUR_QUERY_ERROR))
    return 0;

  if (STREQ(parse_struct->curkey, AUR_ID))
    parse_struct->aurpkg->id = atoi(val);

  else if (STREQ(parse_struct->curkey, AUR_NAME))
    parse_struct->aurpkg->name = strndup(val, size);

  else if (STREQ(parse_struct->curkey, AUR_VER))
    parse_struct->aurpkg->ver = strndup(val, size);

  else if (STREQ(parse_struct->curkey, AUR_CAT))
    parse_struct->aurpkg->cat = atoi(val);

  else if (STREQ(parse_struct->curkey, AUR_DESC))
    parse_struct->aurpkg->desc = strndup(val, size);

  else if (STREQ(parse_struct->curkey, AUR_URL))
    parse_struct->aurpkg->url = strndup(val, size);

  else if (STREQ(parse_struct->curkey, AUR_URLPATH))
    parse_struct->aurpkg->urlpath = strndup(val, size);

  else if (STREQ(parse_struct->curkey, AUR_LICENSE))
    parse_struct->aurpkg->lic = strndup(val, size);

  else if (STREQ(parse_struct->curkey, AUR_VOTES))
    parse_struct->aurpkg->votes = atoi(val);

  else if (STREQ(parse_struct->curkey, AUR_OOD))
    parse_struct->aurpkg->ood = strncmp(val, "1", 1) == 0 ? 1 : 0;


  return 1;
}

static int json_map_key(void *ctx, const unsigned char *data, unsigned int size) {
  struct yajl_parse_struct *parse_struct = (struct yajl_parse_struct*)ctx;

  strncpy(parse_struct->curkey, (const char*)data, size);
  parse_struct->curkey[size] = '\0';

  return 1;
}

static int json_start_map(void *ctx) {
  struct yajl_parse_struct *parse_struct = (struct yajl_parse_struct*)ctx;

  if (parse_struct->json_depth++ >= 1)
    parse_struct->aurpkg = aur_pkg_new();

  return 1;
}


static int json_end_map(void *ctx) {
  struct yajl_parse_struct *parse_struct = (struct yajl_parse_struct*)ctx;

  if (! --(parse_struct->json_depth)) {
    aur_pkg_free(parse_struct->aurpkg);
    return 0;
  }

  *parse_struct->pkg_list = alpm_list_add_sorted(*parse_struct->pkg_list,
                                                 parse_struct->aurpkg,
                                                 aur_pkg_cmp);
  parse_struct->aurpkg = NULL;

  return 1;
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
  return realsize;
}

alpm_list_t *aur_fetch_json(const char *url) {
  CURLcode curlstat;
  long httpcode;
  struct yajl_parse_struct *parse_struct;
  yajl_handle hand;
  alpm_list_t *results;

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
    if (config->color)
      cfprintf(stderr, "%<::%> curl: %s\n", config->colors->error, curl_easy_strerror(curlstat));
    else
      fprintf(stderr, "!! curl: %s\n", curl_easy_strerror(curlstat));
  } else {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
    if (httpcode != 200) {
      if (config->color)
        cfprintf(stderr, "%<::%> curl: server responded with code %l\n",
          config->colors->error, httpcode); 
      else
        fprintf(stderr, "!! curl: server responded with code %ld\n", httpcode);
    }
  }

  aur_pkg_free(parse_struct->aurpkg);
  free(parse_struct);
  yajl_parse_complete(hand);
  yajl_free(hand);

  return results;
}

/* vim: set ts=2 sw=2 et: */

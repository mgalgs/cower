#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#include "aur.h"
#include "curl.h"
#include "package.h"
#include "util.h"
#include "yajl.h"

static yajl_handle hand;
static alpm_list_t *pkg_list;

static struct yajl_parse_struct {
  struct aur_pkg_t *aurpkg;
  char curkey[32];
  int json_depth;
} parse_struct;

static int json_string(void *ctx, const unsigned char *data, unsigned int size) {
  const char *val = (const char*)data;

  if(STREQ(parse_struct.curkey, AUR_QUERY_TYPE) && 
     strncmp(val, AUR_QUERY_ERROR, strlen(AUR_QUERY_ERROR)) == 0) {
    return 0;
  }

  if (STREQ(parse_struct.curkey, AUR_ID))
    parse_struct.aurpkg->id = strndup(val, size);
  else if (STREQ(parse_struct.curkey, AUR_NAME))
    parse_struct.aurpkg->name = strndup(val, size);
  else if (STREQ(parse_struct.curkey, AUR_VER))
    parse_struct.aurpkg->ver = strndup(val, size);
  else if (STREQ(parse_struct.curkey, AUR_CAT))
    parse_struct.aurpkg->cat = strndup(val, size);
  else if (STREQ(parse_struct.curkey, AUR_DESC))
    parse_struct.aurpkg->desc = strndup(val, size);
  else if (STREQ(parse_struct.curkey, AUR_URL))
    parse_struct.aurpkg->url = strndup(val, size);
  else if (STREQ(parse_struct.curkey, AUR_URLPATH))
    parse_struct.aurpkg->urlpath = strndup(val, size);
  else if (STREQ(parse_struct.curkey, AUR_LICENSE))
    parse_struct.aurpkg->lic = strndup(val, size);
  else if (STREQ(parse_struct.curkey, AUR_VOTES))
    parse_struct.aurpkg->votes = strndup(val, size);
  else if (STREQ(parse_struct.curkey, AUR_OOD))
    parse_struct.aurpkg->ood = strncmp(val, "1", 1) == 0 ? 1 : 0;

  return 1;
}

static int json_map_key(void *ctx, const unsigned char *data, unsigned int size) {
  strncpy(parse_struct.curkey, (const char*)data, size);
  parse_struct.curkey[size] = '\0';

  return 1;
}

static int json_start_map(void *ctx) {
  if (parse_struct.json_depth++ >= 1)
    parse_struct.aurpkg = aur_pkg_new();

  return 1;
}


static int json_end_map(void *ctx) {
  if (! --(parse_struct.json_depth)) {
    aur_pkg_free(parse_struct.aurpkg);
    return 0;
  }

  pkg_list = alpm_list_add_sorted(pkg_list, parse_struct.aurpkg, aur_pkg_cmp);
  parse_struct.aurpkg = NULL;

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
  size_t realsize = size * nmemb;
  yajl_parse(hand, ptr, realsize);
  return realsize;
}

alpm_list_t *aur_fetch_json(const char *url) {
  yajl_gen g;
  /* yajl_status yajlstat; */
  CURLcode curlstat;
  long httpcode;

  parse_struct.aurpkg = NULL;
  parse_struct.json_depth = 0;
  pkg_list = NULL;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_yajl);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

  g = yajl_gen_alloc(NULL, NULL);

  hand = yajl_alloc(&callbacks, NULL, NULL, (void *) g);

  curlstat = curl_easy_perform(curl);
  if (curlstat != 0) {
    fprintf(stderr, "curl error: unable to read data from %s\n", url);
    goto cleanup;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode != 200) {
    fprintf(stderr, "curl error: server responded with code %ld\n", httpcode);
    goto cleanup;
  };


  aur_pkg_free(parse_struct.aurpkg);

cleanup:
  yajl_parse_complete(hand);

  yajl_gen_free(g);
  yajl_free(hand);

  return pkg_list;
}

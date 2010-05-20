#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aur.h"
#include "curl.h"
#include "package.h"
#include "pacman.h"
#include "util.h"
#include "yajl.h"

static yajl_handle hand;

static char curkey[32];
static int json_depth = 0;

static struct aur_pkg_t *aurpkg;
static alpm_list_t *pkg_list;

static int json_string(void *ctx, const unsigned char *data, unsigned int size) {
  char val[256];

  strncpy(val, (const char*)data, size);
  val[size] = '\0';

  if (STREQ(curkey, AUR_ID))
    aurpkg->id = strdup(val);
  else if (STREQ(curkey, AUR_NAME))
    aurpkg->name = strdup(val);
  else if (STREQ(curkey, AUR_VER))
    aurpkg->ver = strdup(val);
  else if (STREQ(curkey, AUR_CAT))
    aurpkg->cat = strdup(val);
  else if (STREQ(curkey, AUR_DESC))
    aurpkg->desc = strdup(val);
  else if (STREQ(curkey, AUR_URL))
    aurpkg->url = strdup(val);
  else if (STREQ(curkey, AUR_URLPATH))
    aurpkg->urlpath = strdup(val);
  else if (STREQ(curkey, AUR_LICENSE))
    aurpkg->lic = strdup(val);
  else if (STREQ(curkey, AUR_VOTES))
    aurpkg->votes = strdup(val);
  else if (STREQ(curkey, AUR_OOD))
    aurpkg->ood = STREQ(val, "1") ? 1 : 0;

  return 1;
}

static int json_map_key(void *ctx, const unsigned char *data, unsigned int size) {
  strncpy(curkey, (const char*)data, size);
  curkey[size] = '\0';

  return 1;
}

static int json_start_map(void *ctx) {
  if (json_depth++ >= 1)
    aurpkg = aur_pkg_new();

  return 1;
}


static int json_end_map(void *ctx) {
  if (! --json_depth) {
    aur_pkg_free(aurpkg);
    return 0;
  }

  pkg_list = alpm_list_add_sorted(pkg_list, aurpkg, aur_pkg_cmp);
  aurpkg = NULL;

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
  //yajl_status stat;
  CURLcode status;

  aurpkg = NULL;
  pkg_list = NULL;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_yajl);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

  g = yajl_gen_alloc(NULL, NULL);

  hand = yajl_alloc(&callbacks, NULL, NULL, (void *) g);

  status = curl_easy_perform(curl);

  yajl_parse_complete(hand);

  yajl_gen_free(g);
  yajl_free(hand);

  return pkg_list;
}

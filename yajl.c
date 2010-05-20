#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <alpm_list.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include "aur.h"
#include "curl.h"
#include "package.h"
#include "util.h"
#include "yajl.h"

static yajl_handle hand;

static char curkey[32];
static int json_depth = 0;

static struct pkg_t *aurpkg = NULL;
static alpm_list_t *pkg_list = NULL;

static int json_string(void *ctx, const unsigned char *data, unsigned int size) {
  char val[256];

  strncpy(val, (const char*)data, size);
  val[stringLen] = '\0';

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
  else if (STREQ(curkey, AUR_LOC))
    aurpkg->loc = strdup(val);
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
  curkey[stringLen] = '\0';

  return 1;
}

static int json_start_map(void *ctx) {
  if (json_depth++ >= 1)
    aurpkg = aur_pkg_new(NULL);

  return 1;
}


static int json_end_map(void *ctx) {
  if (! --json_depth) {
    aur_pkg_free(aurpkg);
    return 0;
  }

  pkg_list = alpm_list_add(pkg_list, aurpkg);
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

/*
int main(int argc, char ** argv) {
  yajl_gen g;
  yajl_status stat;
  CURLcode status;

  curl_easy_setopt(curl, CURLOPT_URL, AUR_SEARCH);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_yajl);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

  g = yajl_gen_alloc(NULL, NULL);

  hand = yajl_alloc(&callbacks, NULL, NULL, (void *) g);

  status = curl_easy_perform(curl);

  stat = yajl_parse_complete(hand);
  if (stat != yajl_status_ok && stat != yajl_status_insufficient_data) {
    fprintf(stderr, "An error occurred while parsing the reply from the AUR.\n");
  }

  yajl_gen_free(g);
  yajl_free(hand);

  curl_easy_cleanup(curl);
  curl_global_cleanup();

  alpm_list_t *i;
  for (i = pkg_list; i; i = i->next) {
    print_pkg_info((struct pkg_t*)i->data);
    putchar('\n');
  }

  alpm_list_free_inner(pkg_list, aur_pkg_free);
  alpm_list_free(pkg_list);

  return 0;
}
*/

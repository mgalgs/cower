/* Copyright (c) 2010 Dave Reisner
 *
 * query.c
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

#include "aur.h"
#include "pkgbuild.h"
#include "download.h"
#include "conf.h"
#include "curl.h"
#include "query.h"
#include "util.h"
#include "yajl.h"

alpm_list_t *query_aur_rpc(const char *query_type, const char* arg) {
  char *url, *escaped;
  alpm_list_t *ret;

  /* format URL to pass to curl */
  escaped = curl_easy_escape(curl, arg, strlen(arg));
  cwr_asprintf(&url, AUR_RPC_URL, query_type, escaped);
  curl_free(escaped);

  ret = aur_fetch_json(url);

  free(url);

  return(ret); /* needs to be freed in the calling function if not NULL */
}

alpm_list_t *cower_do_query(alpm_list_t *targets, const char *type) {
  alpm_list_t *i, *resultset;

  resultset = NULL;

  for (i = targets; i; i = i->next) {
    if (STREQ(type, AUR_QUERY_TYPE_SEARCH) && strlen(i->data) < 2) {
      cwr_fprintf(stderr, LOG_ERROR, "search string '%s' too short.\n",
          (const char*)i->data);
      continue;
    }

    alpm_list_t *search = query_aur_rpc(type, i->data);

    if (!search) {
      cwr_fprintf(stderr, LOG_ERROR, "no results for \"%s\"\n", (const char*)i->data);
      continue;
    }

    if (STREQ(type, AUR_QUERY_TYPE_INFO) && config->moreinfo) {
      char *url;
      char *escaped, *pkgbuild;
      struct aur_pkg_t *aurpkg = (struct aur_pkg_t*)search->data;

      escaped = curl_easy_escape(curl, aurpkg->name, strlen(aurpkg->name));

      cwr_asprintf(&url, AUR_PKGBUILD_PATH, escaped, escaped);

      pkgbuild = curl_textfile_get(url);
      free(url);

      if (pkgbuild == NULL) {
        cwr_fprintf(stderr, LOG_ERROR, "error fetching pkgbuild\n");
        continue;
      }

      alpm_list_t **pkg_details[PKGDETAIL_MAX] = {
        &aurpkg->depends, &aurpkg->makedepends, &aurpkg->optdepends,
        &aurpkg->provides, &aurpkg->conflicts, &aurpkg->replaces
      };

      pkgbuild_extinfo_get(&pkgbuild, pkg_details, 0);

      free(pkgbuild);
      curl_free(escaped);
    }

    /* Aggregate searches into a single list and remove dupes */
    resultset = alpm_list_mmerge_dedupe(resultset, search, aur_pkg_cmp, aur_pkg_free);
  }

  return(resultset);
}

/* vim: set ts=2 sw=2 et: */

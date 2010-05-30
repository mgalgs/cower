/*
 *  search.c
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

#include "aur.h"
#include "pkgbuild.h"
#include "download.h"
#include "conf.h"
#include "curl.h"
#include "query.h"
#include "util.h"
#include "yajl.h"

alpm_list_t *query_aur_rpc(const char *query_type, const char* arg) {

  char url[AUR_URL_SIZE + 1];
  char *escaped;
  alpm_list_t *ret;

  escaped = curl_easy_escape(curl, arg, strlen(arg));

  /* format URL to pass to curl */
  snprintf(url, AUR_URL_SIZE, AUR_RPC_URL, query_type, escaped);

  ret = aur_fetch_json(url);

  curl_free(escaped);

  return ret; /* needs to be freed in the calling function if not NULL */
}

alpm_list_t *cower_do_query(alpm_list_t *targets, const char *type) {
  alpm_list_t *i, *resultset;

  resultset = NULL;

  for (i = targets; i; i = i->next) {
    if (STREQ(type, AUR_QUERY_TYPE_SEARCH) && strlen(i->data) < 2) {
      if (config->color) {
        cfprintf(stderr, "%<::%> search string '%s' too short.\n",
          config->colors->error, (const char*)i->data);
      } else {
        fprintf(stderr, "!! search string '%s' too short.\n",
          (const char*)i->data);
      }
      continue;
    }

    alpm_list_t *search = query_aur_rpc(type, i->data);

    if (! search) {
      if (config->color) {
        cfprintf(stderr, "%<::%>", config->colors->error);
      } else {
        fprintf(stderr, "!!");
      }
      fprintf(stderr, " no results for \"%s\"\n", (const char*)i->data);

      continue;
    }

    if (STREQ(type, AUR_QUERY_TYPE_INFO) && config->moreinfo) {
      char url[AUR_URL_SIZE + 1];
      char *escaped, *pkgbuild;
      struct aur_pkg_t *aurpkg = (struct aur_pkg_t*)search->data;

      escaped = curl_easy_escape(curl, aurpkg->name, strlen(aurpkg->name));

      snprintf(url, AUR_URL_SIZE, AUR_PKGBUILD_PATH, escaped, escaped);

      pkgbuild = curl_textfile_get(url);

      if (pkgbuild == NULL) {
        fprintf(stderr, "error fetching pkgbuild\n");
        continue;
      }

      alpm_list_t **pkg_details[PKGDETAIL_MAX];
      pkg_details[PKGDETAIL_DEPENDS] = &aurpkg->depends;
      pkg_details[PKGDETAIL_MAKEDEPENDS] = &aurpkg->makedepends;
      pkg_details[PKGDETAIL_OPTDEPENDS] = &aurpkg->optdepends;
      pkg_details[PKGDETAIL_PROVIDES] = &aurpkg->provides;
      pkg_details[PKGDETAIL_CONFLICTS] = &aurpkg->conflicts;
      pkg_details[PKGDETAIL_REPLACES] = &aurpkg->replaces;
      pkgbuild_extinfo_get(&pkgbuild, pkg_details);

      free(pkgbuild);
      curl_free(escaped);
    }

    /* Aggregate searches into a single list and remove dupes */
    resultset = alpm_list_mmerge_dedupe(resultset, search, aur_pkg_cmp, aur_pkg_free);
  }

  return resultset;
}

/* vim: set ts=2 sw=2 et: */

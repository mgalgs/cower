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
#include "download.h"
#include "conf.h"
#include "search.h"
#include "util.h"
#include "yajl.h"

/** 
* @brief send a query to the AUR's rpc interface
*
* @param type   search or info
* @param arg    argument to query
*
* @return       a JSON loaded with the results of the query
*/
alpm_list_t *aur_rpc_query(const char *query_type, const char* arg) {

  char url[AUR_URL_SIZE];

  /* Format URL to pass to curl */
  snprintf(url, AUR_URL_SIZE, AUR_RPC_URL, query_type, arg);

  return aur_fetch_json(url); /* This needs to be freed in the calling function */
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
        fprintf(stderr, "error: search string '%s' too short.\n",
          (const char*)i->data);
      }
      continue;
    }

    alpm_list_t *search = aur_rpc_query(type, i->data);

    if (! search) {
      if (config->color) {
        cfprintf(stderr, "%<::%>", config->colors->error);
      } else {
        fprintf(stderr, "error:");
      }
      fprintf(stderr, " no results for \"%s\"\n", (const char*)i->data);
    }

    /* Aggregate searches into a single list and remove dupes */
    resultset = agg_search_results(resultset, search);
  }

  return resultset;
}

/* vim: set ts=2 sw=2 et: */

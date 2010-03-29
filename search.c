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

/* standard */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* non-standard */
#include <alpm.h>
#include <jansson.h>

/* local */
#include "alpmutil.h"
#include "download.h"
#include "conf.h"
#include "search.h"
#include "util.h"

/** 
* @brief search alpm's local db for package
* 
* @param target alpm_list_t carrying packages to search for
* 
* @return a list of packages fufilling the criteria
*/
alpm_list_t *alpm_query_search(alpm_list_t *target) {

  alpm_list_t *i, *searchlist, *ret = NULL;
  int freelist;

  if (target) { /* If we have a target, search for it */
    searchlist = alpm_db_search(db_local, target);
    freelist = 1;
  } else { /* Otherwise return a pointer to the local cache */
    searchlist = alpm_db_get_pkgcache(db_local);
    freelist = 0;
  }

  if(searchlist == NULL) {
    return NULL;
  }

  for(i = searchlist; i; i = alpm_list_next(i)) {
    pmpkg_t *pkg = alpm_list_getdata(i);

    /* TODO: Reuse alpm_sync_search for this */
    if (!target && is_foreign(pkg)) {
      ret = alpm_list_add(ret, pkg);
    }
  }

  if (freelist) {
    alpm_list_free(searchlist);
  }

  return ret; /* This needs to be freed in the calling function */
}


/** 
* @brief search alpm's sync DBs for a package
* 
* @param target   alpm_list_t with packages to search for
* 
* @return DB package was found in, or NULL
*/
pmdb_t *alpm_sync_search(alpm_list_t *target) {

  pmdb_t *db = NULL;
  alpm_list_t *syncs, *i, *j;

  syncs = alpm_option_get_syncdbs();

  /* Iterating over each sync */
  for (i = syncs; i; i = alpm_list_next(i)) {
    db = alpm_list_getdata(i);

    /* Iterating over each package in sync */
    for (j = alpm_db_get_pkgcache(db); j; j = alpm_list_next(j)) {
      pmpkg_t *pkg = alpm_list_getdata(j);
      if (strcmp(alpm_pkg_get_name(pkg), alpm_list_getdata(target)) == 0) {
        return db;
      }
    }
  }

  return NULL; /* Not found */
}

/** 
* @brief checks for a package by name in pacman's sync DBs
* 
* @param target   package name to find
* 
* @return 1 on success, 0 on failure
*/
int is_in_pacman(const char *target) {

  pmdb_t *found_in;
  alpm_list_t *p = NULL;

  p = alpm_list_add(p, (void*)target);
  found_in = alpm_sync_search(p);

  if (found_in) {
    if (config->color) {
      cprintf("%<%s%> is available in %<%s%>\n",
        WHITE, target, YELLOW, alpm_db_get_name(found_in));
    } else {
      printf("%s is available in %s\n", target, alpm_db_get_name(found_in));
    }

    alpm_list_free(p);
    return TRUE;
  }

  alpm_list_free(p);
  return FALSE;
}
/** 
* @brief send a query to the AUR's rpc interface
* 
* @param type   search or info
* @param arg    argument to query
* 
* @return       a JSON loaded with the results of the query
*/
json_t *aur_rpc_query(int type, const char* arg) {

  char *text;
  char url[AUR_RPC_URL_SIZE];
  json_t *root, *return_type;
  json_error_t error;

  /* Format URL to pass to curl */
  snprintf(url, AUR_RPC_URL_SIZE, AUR_RPC_URL,
    type == AUR_RPC_QUERY_TYPE_INFO ? "info" : "search", arg);

  text = curl_get_json(url);
  if(!text)
    return NULL;

  /* Fetch JSON */
  root = json_loads(text, &error);
  free(text);

  /* Check for error */
  if(!root) {
    if (config->color) {
      cfprintf(stderr, "%<error:%> could not create JSON. Please report this error.\n",
        RED);
    } else {
      fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
    }
    return NULL;
  }

  /* Check return type in JSON */
  return_type = json_object_get(root, "type");
  if (! strcmp(json_string_value(return_type), "error")) {
    json_decref(root);
    return NULL;
  }

  return root; /* This needs to be freed in the calling function */
}


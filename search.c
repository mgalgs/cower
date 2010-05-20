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
#include "pacman.h"
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


/** 
* @brief search for target packages
* 
* @param targets  an alpm_list_t of candidate pacman packages
* 
* @return number of packages available in AUR
*/
int get_pkg_availability(alpm_list_t *targets) {

  alpm_list_t *i;
  int ret = 0;

  /* Iterate over targets packages */
  for (i = targets; i; i = alpm_list_next(i)) {
    pmpkg_t *pmpkg = alpm_list_getdata(i);

    if (config->verbose > 0) {
      if (config->color) {
        cprintf("Checking %<%s%> for updates...\n", WHITE, alpm_pkg_get_name(pmpkg));
      } else {
        printf("Checking %s for updates...\n", alpm_pkg_get_name(pmpkg));
      }
    }

    /* Do I exist in the AUR? */
    alpm_list_t *results = aur_rpc_query(AUR_QUERY_TYPE_INFO, alpm_pkg_get_name(pmpkg));

    if (results == NULL) { /* Not found, next candidate */
      continue;
    }

    const char *remote_ver, *local_ver;
    struct aur_pkg_t *aurpkg;

    aurpkg = (struct aur_pkg_t*)alpm_list_getdata(results);
    remote_ver = aurpkg->ver;
    local_ver = alpm_pkg_get_version(pmpkg);

    /* Version check */
    if (alpm_pkg_vercmp(remote_ver, local_ver) <= 0) {
      aur_pkg_free(aurpkg);
      continue;
    }

    ret++; /* Found an update, increment */
    if (config->op & OP_DL) /* -d found with -u */
      aur_get_tarball(aurpkg);
    else {
      /* TODO: Could I just use pkg->name here? */
      print_pkg_update(alpm_pkg_get_name(pmpkg), local_ver, remote_ver);
    }
  }

  return ret;
}


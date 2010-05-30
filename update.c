/*
 *  update.c
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

#include "aur.h"
#include "conf.h"
#include "download.h"
#include "pacman.h"
#include "query.h"
#include "util.h"

int cower_do_update() {
  int ret;

  alpm_quick_init();
  ret = 0;

  alpm_list_t *foreign_pkgs = alpm_query_foreign();
  if (!foreign_pkgs)
    return ret;

  /* Iterate over targets packages */
  alpm_list_t *i;
  for (i = foreign_pkgs; i; i = i->next) {
    pmpkg_t *pmpkg = i->data;

    if (config->verbose > 0) {
      if (config->color) {
        cprintf("%<::%> Checking %<%s%> for updates...\n", 
          config->colors->info,
          config->colors->pkg, alpm_pkg_get_name(pmpkg));
      } else {
        printf(":: Checking %s for updates...\n", alpm_pkg_get_name(pmpkg));
      }
    }

    /* Do I exist in the AUR? */
    alpm_list_t *results = query_aur_rpc(AUR_QUERY_TYPE_INFO, alpm_pkg_get_name(pmpkg));

    if (!results) /* Not found, next candidate */
      continue;

    const char *remote_ver, *local_ver;
    struct aur_pkg_t *aurpkg;

    aurpkg = (struct aur_pkg_t*)results->data;
    remote_ver = aurpkg->ver;
    local_ver = alpm_pkg_get_version(pmpkg);

    /* Version check */
    if (alpm_pkg_vercmp(remote_ver, local_ver) > 0) {
      ret++; /* Found an update, increment */

      if (config->op & OP_DL) /* -d found with -u */
        download_taurball(aurpkg);
      else {
        print_pkg_update(aurpkg->name, local_ver, remote_ver);
      }
    }

    aur_pkg_free(aurpkg);
    alpm_list_free(results);
  }

  alpm_list_free(foreign_pkgs);

  return ret;
}

/* vim: set ts=2 sw=2 et: */

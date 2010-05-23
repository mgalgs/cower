/*
 *  package.c
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

#include <stdlib.h>
#include <string.h>

#include "package.h"
#include "util.h"

/** 
* @brief callback for comparing aur_pkg_t structs inside an alpm_list_t
* 
* @param p1   left side of the comparison
* @param p2   right side of the comparison
* 
* @return     results of strcmp on names of each aur_pkg_t
*/
int aur_pkg_cmp(const void *p1, const void *p2) {
  struct aur_pkg_t *pkg1 = (struct aur_pkg_t*)p1;
  struct aur_pkg_t *pkg2 = (struct aur_pkg_t*)p2;

  return strcmp((const char*)pkg1->name, (const char*)pkg2->name);
}

/** 
* @brief callback for freeing an aur_pkg_t inside an alpm_list_t
* 
* @param pkg  aur_pkg_t struct inside linked list
*/
void aur_pkg_free(void *pkg) {
  if (!pkg)
    return;

  struct aur_pkg_t *it = (struct aur_pkg_t*)pkg;

  FREE(it->name);
  FREE(it->ver);
  FREE(it->desc);
  FREE(it->url);
  FREE(it->urlpath);
  FREE(it->lic);

  FREE(it);
}

struct aur_pkg_t *aur_pkg_new() {
  struct aur_pkg_t *pkg;

  pkg = calloc(1, sizeof *pkg);

  pkg->name = pkg->ver = pkg->desc = pkg->lic = pkg->url = pkg->urlpath = NULL;
  pkg->id = pkg->cat = pkg->ood = pkg->votes = 0;

  return pkg;
}

/* vim: set ts=2 sw=2 et: */

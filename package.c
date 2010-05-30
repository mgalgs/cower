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

int aur_pkg_cmp(const void *p1, const void *p2) {
  struct aur_pkg_t *pkg1 = (struct aur_pkg_t*)p1;
  struct aur_pkg_t *pkg2 = (struct aur_pkg_t*)p2;

  return strcmp((const char*)pkg1->name, (const char*)pkg2->name);
}

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

  FREELIST(it->depends);
  FREELIST(it->makedepends);
  FREELIST(it->optdepends);
  FREELIST(it->provides);
  FREELIST(it->conflicts);
  FREELIST(it->replaces);

  FREE(it);
}

struct aur_pkg_t *aur_pkg_new() {
  struct aur_pkg_t *pkg;

  pkg = calloc(1, sizeof *pkg);

  pkg->name = pkg->ver = pkg->desc = pkg->lic = pkg->url = pkg->urlpath = NULL;
  pkg->depends = pkg->makedepends = pkg->optdepends = NULL;
  pkg->id = pkg->cat = pkg->ood = pkg->votes = 0;

  return pkg;
}

/* vim: set ts=2 sw=2 et: */

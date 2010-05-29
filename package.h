/*
 *  package.h
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

#ifndef _COWER_PACKAGE_H
#define _COWER_PACKAGE_H

#include <alpm_list.h>

struct aur_pkg_t {
  int id;
  const char *name;
  const char *ver;
  int cat;
  const char *desc;
  const char *url;
  const char *urlpath;
  const char *lic;
  int votes;
  int ood;

  /* Detailed info */
  alpm_list_t *depends;
  alpm_list_t *makedepends;
  alpm_list_t *optdepends;
  alpm_list_t *provides;
  alpm_list_t *conflicts;
  alpm_list_t *replaces;
};

int aur_pkg_cmp(const void*, const void*);
void aur_pkg_free(void*);
struct aur_pkg_t *aur_pkg_new(void);

#endif /* _COWER_PACKAGE_H */

/* vim: set ts=2 sw=2 et: */

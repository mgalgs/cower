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

#include <jansson.h>

#include "package.h"
#include "util.h"

int _aur_pkg_cmp(void *p1, void *p2) {
  return strcmp((const char*)((aur_pkg_t*)p1)->name,
                (const char*)((aur_pkg_t*)p2)->name);
}

void _aur_pkg_free(void *pkg) {
  FREE(((aur_pkg_t*)pkg)->id);
  FREE(((aur_pkg_t*)pkg)->name);
  FREE(((aur_pkg_t*)pkg)->ver);
  FREE(((aur_pkg_t*)pkg)->cat);
  FREE(((aur_pkg_t*)pkg)->desc);
  FREE(((aur_pkg_t*)pkg)->url);
  FREE(((aur_pkg_t*)pkg)->urlpath);
  FREE(((aur_pkg_t*)pkg)->lic);
  FREE(((aur_pkg_t*)pkg)->votes);
}

aur_pkg_t *json_to_aur_pkg(json_t* j) {

  aur_pkg_t *newpkg;

  newpkg = malloc(sizeof(aur_pkg_t));

  newpkg->id = strdup(json_string_value(json_object_get(j, "ID")));
  newpkg->name = strdup(json_string_value(json_object_get(j, "Name")));
  newpkg->ver = strdup(json_string_value(json_object_get(j, "Version")));
  newpkg->cat = strdup(json_string_value(json_object_get(j, "CategoryID")));
  newpkg->desc = strdup(json_string_value(json_object_get(j, "Description")));
  newpkg->url = strdup(json_string_value(json_object_get(j, "URL")));
  newpkg->urlpath = strdup(json_string_value(json_object_get(j, "URLPath")));
  newpkg->lic = strdup(json_string_value(json_object_get(j, "License")));
  newpkg->votes = strdup(json_string_value(json_object_get(j, "NumVotes")));
  newpkg->ood = atoi(json_string_value(json_object_get(j, "OutOfDate")));

  return newpkg;
}

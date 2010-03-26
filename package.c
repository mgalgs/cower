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

/* standard */
#include <stdlib.h>
#include <string.h>

/* non-standard */
#include <alpm.h>
#include <jansson.h>

/* local */
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
int _aur_pkg_cmp(void *p1, void *p2) {
  return strcmp((const char*)((aur_pkg_t*)p1)->name,
                (const char*)((aur_pkg_t*)p2)->name);
}

/** 
* @brief callback for freeing an aur_pkg_t inside an alpm_list_t
* 
* @param pkg  aur_pkg_t struct inside linked list
*/
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
  
  FREE(pkg);
}

/** 
* @brief convert a JSON to an aur_pkg_t
* 
* @param j    JSON to convert
* 
* @return     aur_pkg_t representation of JSON
*/
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

/*
 *  alpmutil.c
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
#include <string.h>

/* non-standard */
#include <alpm.h>
#include <jansson.h>

/* local */
#include "alpmutil.h"
#include "conf.h"
#include "package.h"
#include "util.h"


/** 
* @brief modified merge sort which deletes duplicates
* 
* @param left     left side of merge
* @param right    right side of merge
* @param fn       callback function for deleting data
* 
* @return the merged list
*/
alpm_list_t *alpm_list_mmerge_dedupe(alpm_list_t *left, alpm_list_t *right, alpm_list_fn_cmp fn) {

  alpm_list_t *lp, *newlist;
  int compare;

  if (left == NULL)
    return right;
  if (right == NULL)
    return left;

  do {
    compare = fn(left->data, right->data);
    if (compare > 0) {
      newlist = right;
      right = right->next;
    }
    else if (compare < 0) {
      newlist = left;
      left = left->next;
    } else {
      left = alpm_list_remove_item(left, left, _aur_pkg_free);
    }
  } while (compare == 0);

  newlist->prev = NULL;
  newlist->next = NULL;
  lp = newlist;

  while ((left != NULL) && (right != NULL)) {
    compare = fn(left->data, right->data);
    if (compare < 0) {
      lp->next = left;
      left->prev = lp;
      left = left->next;
    }
    else if (compare > 0) {
      lp->next = right;
      right->prev = lp;
      right = right->next;
    } else {
      left = alpm_list_remove_item(left, left, _aur_pkg_free);
      continue;
    }
    lp = lp->next;
    lp->next = NULL;
  }
  if (left != NULL) {
    lp->next = left;
    left->prev = lp;
  }
  else if (right != NULL) {
    lp->next = right;
    right->prev = lp;
  }

  while(lp && lp->next) {
    lp = lp->next;
  }
  newlist->prev = lp;

  return(newlist);
}

/** 
* @brief Remove a specific node from a list
* 
* @param haystack     list to remove from
* @param needle       node within list to remove
* @param fn           comparison function
* 
* @return the node following the removed node
*/
alpm_list_t *alpm_list_remove_item(alpm_list_t *haystack, alpm_list_t *needle, alpm_list_fn_free fn) {

  alpm_list_t *next = NULL;

  if (needle == haystack) {
    haystack = needle->next;
    if (haystack) {
      haystack->prev = needle->prev;
    }
    needle->prev = NULL;
  } else if (needle == haystack->prev) {
    if (needle->prev) {
      needle->prev->next = needle->next;
      haystack->prev = needle->prev;
      needle->prev = NULL;
    }
  } else {
    if (needle->next) {
      needle->next->prev = needle->prev;
    }
    if (needle->prev) {
      needle->prev->next = needle->next;
    }
  }

  next = needle->next;

  if (needle->data) {
    fn(needle->data);
  }

  free(needle);

  return next;
}

/** 
* @brief initialize alpm and register default DBs
*/
void alpm_quick_init() {

  /* TODO: Parse pacman config */
  alpm_initialize();
  alpm_option_set_root("/");
  alpm_option_set_dbpath("/var/lib/pacman/");
  alpm_db_register_sync("testing");
  alpm_db_register_sync("community-testing");
  alpm_db_register_sync("core");
  alpm_db_register_sync("extra");
  alpm_db_register_sync("community");
  db_local = alpm_db_register_local();
}

/** 
* @brief determine is a package is in pacman's sync DBs
* 
* @param pkg    package to search for
* 
* @return TRUE if foreign, else FALSE
*/
int is_foreign(pmpkg_t *pkg) {

  const char *pkgname = alpm_pkg_get_name(pkg);
  alpm_list_t *j;
  alpm_list_t *sync_dbs = alpm_option_get_syncdbs();

  for(j = sync_dbs; j; j = alpm_list_next(j)) {
    pmdb_t *db = alpm_list_getdata(j);
    pmpkg_t *findpkg = alpm_db_get_pkg(db, pkgname);
    if(findpkg) {
      return FALSE;
    }
  }

  return TRUE;
}


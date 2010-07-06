/*
 *  pacman.c
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

#define _GNU_SOURCE

#include <linux/limits.h>
#include <stdio.h>
#include <string.h>

#include "pacman.h"
#include "conf.h"
#include "util.h"

static int is_foreign(pmpkg_t *pkg) {
  const char *pkgname;
  alpm_list_t *i, *syncs;

  pkgname = alpm_pkg_get_name(pkg);
  syncs = alpm_option_get_syncdbs();

  for (i = syncs; i; i = i->next) {
    if (alpm_db_get_pkg(i->data, pkgname))
      return FALSE;
  }

  return TRUE;
}

alpm_list_t *alpm_list_mmerge_dedupe(alpm_list_t *left, alpm_list_t *right, alpm_list_fn_cmp fn, alpm_list_fn_free fnf) {

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
    } else if (compare < 0) {
      newlist = left;
      left = left->next;
    } else {
      left = alpm_list_remove_item(left, left, fnf);
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
      left = alpm_list_remove_item(left, left, fnf);
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

alpm_list_t *alpm_list_remove_item(alpm_list_t *listhead, alpm_list_t *target, alpm_list_fn_free fn) {

  alpm_list_t *next = NULL;

  if (target == listhead) {
    listhead = target->next;
    if (listhead)
      listhead->prev = target->prev;

    target->prev = NULL;
  } else if (target == listhead->prev) {
    if (target->prev) {
      target->prev->next = target->next;
      listhead->prev = target->prev;
      target->prev = NULL;
    }
  } else {
    if (target->next)
      target->next->prev = target->prev;

    if (target->prev)
      target->prev->next = target->next;
  }

  next = target->next;

  if (target->data)
    fn(target->data);

  free(target);

  return next;
}

alpm_list_t *alpm_query_foreign() {
  alpm_list_t *i, *ret = NULL;

  for(i = alpm_db_get_pkgcache(db_local); i; i = i->next) {
    pmpkg_t *pkg = i->data;

    if (is_foreign(pkg))
      ret = alpm_list_add(ret, pkg);
  }

  return ret; /* This needs to be freed in the calling function */
}

void alpm_quick_init() {
  if (config->verbose >= 2)
    printf("::DEBUG:: Initializing alpm\n");

  FILE *pacfd;
  char *ptr, *section = NULL;
  char line[PATH_MAX + 1];

  alpm_initialize();

  alpm_option_set_root("/");
  alpm_option_set_dbpath("/var/lib/pacman");
  db_local = alpm_db_register_local();

  pacfd = fopen(PACCONF, "r");
  if (! pacfd) {
    if (config->color)
      cfprintf(stderr, "%<::%> could not locate pacman config.\n",
        config->colors->error);
    else
      fprintf(stderr, "!! could not locate pacman config.\n");
    return;
  }

  while (fgets(line, PATH_MAX, pacfd)) {
    alpm_list_t *list = NULL, *item = NULL;
    strtrim(line);

    /* strip out comments */
    if (line[0] == '#' || strlen(line) == 0)
      continue;
    if ((ptr = strchr(line, '#')))
      *ptr = '\0';

    /* found a [section] */
    if (line[0] == '[' || line[(strlen(line) - 1)] == ']') {
      if (section)
        free(section);

      /* dupe the line without the square brackets */
      section = strndup(line + 1, strlen(line) - 2);

      if (strcmp(section, "options") != 0)
        alpm_db_register_sync(section);
    } else { /* plain option */
      char *key;
      key = line;
      ptr = line;
      strsep(&ptr, "=");
      strtrim(key);
      strtrim(ptr);

      if (STREQ(key, "RootDir"))
        alpm_option_set_root(ptr);
      else if (STREQ(key, "DBPath"))
        alpm_option_set_dbpath(ptr);
      else if (STREQ(key, "IgnorePkg")) {
        list = strsplit(ptr, ' ');
        for (item = list; item; item = item->next) {
          if (alpm_list_find_str(config->ignorepkgs, item->data) == NULL) {
            config->ignorepkgs = alpm_list_add(config->ignorepkgs, strdup(item->data));
          }
        }
      }
    }
  }

  free(section);
  fclose(pacfd);
}

pmdb_t *alpm_sync_search(alpm_list_t *target) {

  pmdb_t *db = NULL;
  alpm_list_t *syncs, *i, *j;

  syncs = alpm_option_get_syncdbs();

  /* Iterating over each sync */
  for (i = syncs; i; i = i->next) {
    db = i->data;

    /* Iterating over each package in sync */
    for (j = alpm_db_get_pkgcache(db); j; j = j->next) {
      pmpkg_t *pkg = j->data;
      if (strcmp(alpm_pkg_get_name(pkg), alpm_list_getdata(target)) == 0) {
        return db;
      }
    }
  }

  return NULL; /* Not found */
}

int is_in_pacman(const char *target) {

  pmdb_t *found_in;
  alpm_list_t *p = NULL;

  p = alpm_list_add(p, (void*)target);
  found_in = alpm_sync_search(p);

  if (found_in) {
    if (config->color) {
      cprintf("%<::%> %<%s%> is available in %<%s%>\n",
        config->colors->warn,
        config->colors->pkg, target,
        config->colors->warn, alpm_db_get_name(found_in));
    } else {
      printf("^^ %s is available in %s\n", target, alpm_db_get_name(found_in));
    }

    alpm_list_free(p);
    return TRUE;
  }

  alpm_list_free(p);
  return FALSE;
}

/* vim: set ts=2 sw=2 et: */

/* Copyright (c) 2010 Dave Reisner
 *
 * pacman.c
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#define _GNU_SOURCE
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "util.h"
#include "pacman.h"
#include "conf.h"

static int is_foreign(pmpkg_t *pkg) {
  const char *pkgname;
  alpm_list_t *i;

  pkgname = alpm_pkg_get_name(pkg);

  for (i = alpm_option_get_syncdbs(); i; i = i->next) {
    if (alpm_db_get_pkg(i->data, pkgname)) {
      return(0);
    }
  }

  return(1);
}

alpm_list_t *alpm_list_mmerge_dedupe(alpm_list_t *left, alpm_list_t *right, alpm_list_fn_cmp fn, alpm_list_fn_free fnf) {
  alpm_list_t *lp, *newlist;
  int compare;

  if (left == NULL) {
    return(right);
  }

  if (right == NULL) {
    return(left);
  }

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
    if (listhead) {
      listhead->prev = target->prev;
    }

    target->prev = NULL;
  } else if (target == listhead->prev) {
    if (target->prev) {
      target->prev->next = target->next;
      listhead->prev = target->prev;
      target->prev = NULL;
    }
  } else {
    if (target->next) {
      target->next->prev = target->prev;
    }

    if (target->prev) {
      target->prev->next = target->next;
    }
  }

  next = target->next;

  if (target->data) {
    fn(target->data);
  }

  free(target);

  return(next);
}

alpm_list_t *alpm_query_foreign() {
  alpm_list_t *i, *ret = NULL;

  for(i = alpm_db_get_pkgcache(db_local); i; i = i->next) {
    pmpkg_t *pkg = i->data;

    if (is_foreign(pkg)) {
      ret = alpm_list_add(ret, pkg);
    }
  }

  return(ret); /* This needs to be freed in the calling function */
}

void alpm_quick_init() {
  FILE *fd;
  char *ptr, *section = NULL;
  char line[PATH_MAX + 1];

  cwr_printf(LOG_DEBUG, "Initializing alpm\n");
  alpm_initialize();

  alpm_option_set_root("/");
  alpm_option_set_dbpath("/var/lib/pacman");
  db_local = alpm_db_register_local();

  fd = fopen(PACCONF, "r");
  if (!fd) {
    cwr_fprintf(stderr, LOG_ERROR, "could not locate pacman config.\n");
    return;
  }

  while (fgets(line, PATH_MAX, fd)) {
    alpm_list_t *list = NULL, *item = NULL;
    strtrim(line);

    /* strip out comments */
    if (line[0] == '#' || strlen(line) == 0) {
      continue;
    }
    if ((ptr = strchr(line, '#'))) {
      *ptr = '\0';
    }

    /* found a [section] */
    if (line[0] == '[' || line[(strlen(line) - 1)] == ']') {
      if (section) {
        free(section);
      }

      /* dupe the line without the square brackets */
      section = strndup(line + 1, strlen(line) - 2);

      if (strcmp(section, "options") != 0) {
        alpm_db_register_sync(section);
      }
    } else { /* plain option */
      char *key;
      key = line;
      ptr = line;
      strsep(&ptr, "=");
      strtrim(key);
      strtrim(ptr);

      if (STREQ(key, "RootDir")) {
        alpm_option_set_root(ptr);
      } else if (STREQ(key, "DBPath")) {
        alpm_option_set_dbpath(ptr);
      } else if (STREQ(key, "IgnorePkg")) {
        list = strsplit(ptr, ' ');
        for (item = list; item; item = item->next) {
          if (alpm_list_find_str(config->ignorepkgs, item->data) == NULL) {
            config->ignorepkgs = alpm_list_add(config->ignorepkgs, strdup(item->data));
          }
        }
        FREELIST(list);
      }
    }
  }

  free(section);
  fclose(fd);
}

int alpm_provides_pkg(const char *pkgname) {
  pmdb_t *db;
  alpm_list_t *i;

  for (i = alpm_option_get_syncdbs(); i; i = alpm_list_next(i)) {
    db = alpm_list_getdata(i);
    if (alpm_db_get_pkg(db, pkgname)) {
      cwr_fprintf(stderr, LOG_WARN, "%s%s%s is available in %s%s%s\n",
          config->strings->pkg, pkgname, config->strings->c_off,
          config->strings->repo, alpm_db_get_name(db), config->strings->c_off);
      return(1);
    }
  }

  return(0);
}

/* vim: set ts=2 sw=2 et: */

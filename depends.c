/*
 *  depends.h
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
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "alpmutil.h"
#include "conf.h"
#include "depends.h"
#include "download.h"
#include "search.h"
#include "util.h"

alpm_list_t *parse_bash_array(alpm_list_t *deplist, char *startdep) {

  char *token;

  while ((token = strsep(&startdep, " ")) != NULL) {
    if (strlen(token) <= 1) continue; /* Kludgy */
    if (*token == '\'' || *token == '\"')
      token++;
    char *i;
    for (i = token; *i != '\0'; i++) {
      switch (*i) {
      case '=':
      case '<':
      case '>':
      case '\'':
        *i = '\0';
        break;
      }
    }
    deplist = alpm_list_add(deplist, strdup(token));
  }

  return deplist;

}

alpm_list_t *parsedeps(const char *pkgbuild, alpm_list_t *deplist) {
  FILE* fd;
  char *deps, *tmplist, *buffer, *bptr;

  buffer = calloc(1, BUFSIZ + 1);

  fd = fopen(pkgbuild, "r");
  fread(buffer, sizeof(char), BUFSIZ, fd); 

  bptr = buffer;

  while ((deps = strstr(bptr, "depends=(")) != NULL) {
    tmplist = strndup(deps + 9, strchr(deps, ')') - deps);
    deplist = parse_bash_array(deplist, tmplist);
    free(tmplist);
    bptr = tmplist + strlen(tmplist) + 1;
  }

  fclose(fd);
  free(buffer);

  return deplist;
}

int get_pkg_dependencies(const char *pkg, const char *pkgbuild_path) {

  if (! file_exists(pkgbuild_path)) {
    return -1;
  }

  alpm_list_t *deplist = NULL;
  deplist = parsedeps(pkgbuild_path, deplist);

  if (! config->quiet) {
    if (config->color) {
      cprintf("\n%<::%> Attempting to fetch uninstalled dependencies for %<%s%>...\n",
        BLUE, WHITE, pkg);
    } else {
      printf("\n:: Attempting to fetch uninstalled dependencies for %s...\n", pkg);
    }
  }

  alpm_list_t *i;
  for (i = deplist; i; i = alpm_list_next(i)) {
    const char* depend = i->data;

    if (alpm_db_get_pkg(db_local, depend)) { /* installed */
      continue;
    }
    if (is_in_pacman(depend)) { /* available in pacman */
      continue;
    }
    /* if we're here, we need to check the AUR */
    json_t *infojson = aur_rpc_query(AUR_QUERY_TYPE_INFO, depend);
    if (infojson) {
      aur_get_tarball(infojson);
    }
    json_decref(infojson);

  }

  /* Cleanup */
  alpm_list_free_inner(deplist, (alpm_list_fn_free)free);
  alpm_list_free(deplist);

  return 0;
}


/*
 *  depends.c
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
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "aur.h"
#include "conf.h"
#include "depends.h"
#include "download.h"
#include "search.h"
#include "util.h"

static alpm_list_t *parse_bash_array(alpm_list_t *deplist, char *deparray) {
  char *token;

  token = strtok(deparray, " \n");
  while (token) {
    if (*token == '\'' || *token == '\"')
      token++;

    *(token + strcspn(token, "=<>\'\"")) = '\0';

    if (config->verbose >= 2)
      fprintf(stderr, "::DEBUG Adding Depend: %s\n", token);

    deplist = alpm_list_add(deplist, strdup(token));

    token = strtok(NULL, " \n");
  }

  return deplist;
}

static alpm_list_t *pkgbuild_get_deps(const char *pkgbuild, alpm_list_t *deplist) {
  FILE* fd;
  char *buffer, *bptr, *arraystart, *arrayend;

  buffer = calloc(1, filesize(pkgbuild) + 1);

  fd = fopen(pkgbuild, "r");
  fread(buffer, sizeof(char), BUFSIZ, fd); 
  bptr = buffer;

  /* This catches depends as well as makedepends.
   * It's valid for multi package files as well,
   * even though the AUR doesn't support them. */
  while ((arraystart = strstr(bptr, PKGBUILD_DEPENDS)) != NULL) {
    arrayend = strchr(arraystart, ')');
    *arrayend = '\0';

    /* This is a bit magical; however, if it fails, the PKGBUILD isn't valid Bash */
    if (strncmp(arraystart - 3, PKGBUILD_OPTDEPENDS, strlen(PKGBUILD_OPTDEPENDS)) != 0)
      deplist = parse_bash_array(deplist, arraystart + 9);

    bptr = arrayend + 1;
  }
  fclose(fd);

  free(buffer);

  return deplist;
}

int get_pkg_dependencies(const char *pkg) {
  const char *dir;
  char *pkgbuild_path;
  int ret = 0;

  if (config->download_dir == NULL)
    dir = getcwd(NULL, 0);
  else
    dir = realpath(config->download_dir, NULL);

  pkgbuild_path = calloc(1, PATH_MAX + 1);
  snprintf(pkgbuild_path, strlen(dir) + strlen(pkg) + 11, "%s/%s/PKGBUILD", dir, pkg);

  alpm_list_t *deplist = NULL;
  deplist = pkgbuild_get_deps(pkgbuild_path, deplist);

  if (!config->quiet && config->verbose >= 1) {
    if (config->color)
      cprintf("\n%<::%> Fetching uninstalled dependencies for %<%s%>...\n",
        config->colors->info, config->colors->pkg, pkg);
    else
      printf("\n:: Fetching uninstalled dependencies for %s...\n", pkg);
  }

  alpm_list_t *i = deplist;
  for (i = deplist; i; i = i->next) {
    const char* depend = i->data;
    if (config->verbose >= 2)
      printf("::DEBUG Attempting to find %s\n", depend);

    if (alpm_db_get_pkg(db_local, depend)) { /* installed */
      if (config->verbose >= 2)
        printf("::DEBUG %s is installed\n", depend);

      continue;
    }

    if (is_in_pacman(depend)) /* available in pacman */
      continue;

    /* if we're here, we need to check the AUR */
    alpm_list_t *results = aur_rpc_query(AUR_QUERY_TYPE_INFO, depend);
    if (results) {

      if (config->verbose >= 2)
        printf("::DEBUG %s is in the AUR\n", depend);

      ret++;
      aur_get_tarball(results->data);

      aur_pkg_free(results->data);
      alpm_list_free(results);
    }
    /* Silently ignore packages that can't be found anywhere */
  }

  /* Cleanup */
  FREELIST(deplist);
  FREE(dir);
  FREE(pkgbuild_path);

  return ret;
}

/* vim: set ts=2 sw=2 et: */

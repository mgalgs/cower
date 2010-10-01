/* Copyright (c) 2010 Dave Reisner
 *
 * pkgbuild.c
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

#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "aur.h"
#include "conf.h"
#include "pkgbuild.h"
#include "download.h"
#include "query.h"
#include "util.h"

static alpm_list_t *parse_bash_array(alpm_list_t *deplist, char **deparray, int stripver) {
  char *token;

  /* XXX: This will fail sooner or later */
  if (strchr(*deparray, ':')) { /* we're dealing with optdepdends */
    for (token = strtok(*deparray, "\\\'\"\n"); token; token = strtok(NULL, "\\\'\"\n")) {
      ltrim(token);
      if (strlen(token)) {
        if (config->verbose >= 2)
          fprintf(stderr, "::DEBUG Adding Depend: %s\n", token);
        deplist = alpm_list_add(deplist, strdup(token));
      }
    }
    return deplist;
  }

  for (token = strtok(*deparray, " \n"); token; token = strtok(NULL, " \n")) {
    ltrim(token);
    if (strchr("\'\"", *token))
      token++;

    if (stripver)
      *(token + strcspn(token, "=<>\"\'")) = '\0';
    else {
      char *ptr = token + strlen(token) - 1;
      if (strchr("\'\"", *ptr))
        *ptr = '\0';
    }

    /* some people feel compelled to escape newlines inside arrays. these people suck. */
    if STREQ(token, "\\")
      continue;

    if (config->verbose >= 2)
      fprintf(stderr, "::DEBUG Adding Depend: %s\n", token);

    if (alpm_list_find_str(deplist, token) == NULL)
      deplist = alpm_list_add(deplist, strdup(token));

  }

  return deplist;
}

void pkgbuild_extinfo_get(char **pkgbuild, alpm_list_t **details[], int stripver) {
  char *lineptr, *arrayend;

  lineptr = *pkgbuild;

  do {
    strtrim(++lineptr);
    if (*lineptr == '#')
      continue;

    alpm_list_t **deplist;
    if (STR_STARTS_WITH(lineptr, PKGBUILD_DEPENDS))
      deplist = details[PKGDETAIL_DEPENDS];
    else if (STR_STARTS_WITH(lineptr, PKGBUILD_MAKEDEPENDS))
      deplist = details[PKGDETAIL_MAKEDEPENDS];
    else if (STR_STARTS_WITH(lineptr, PKGBUILD_OPTDEPENDS))
      deplist = details[PKGDETAIL_OPTDEPENDS];
    else if (STR_STARTS_WITH(lineptr, PKGBUILD_PROVIDES))
      deplist = details[PKGDETAIL_PROVIDES];
    else if (STR_STARTS_WITH(lineptr, PKGBUILD_REPLACES))
      deplist = details[PKGDETAIL_REPLACES];
    else if (STR_STARTS_WITH(lineptr, PKGBUILD_CONFLICTS))
      deplist = details[PKGDETAIL_CONFLICTS];
    else
      continue;

    arrayend = strchr(lineptr, ')');
    *arrayend  = '\0';

    if (deplist) {
      char *arrayptr = strchr(lineptr, '(') + 1;
      *deplist = parse_bash_array(*deplist, &arrayptr, stripver);
    }

    lineptr = arrayend + 1;
  } while ((lineptr = strchr(lineptr, '\n')));
}

int get_pkg_dependencies(const char *pkg) {
  const char *dir;
  char *pkgbuild_path, *buffer;
  int ret = 0;
  alpm_list_t *deplist = NULL;

  if (config->download_dir == NULL)
    dir = getcwd(NULL, 0);
  else
    dir = realpath(config->download_dir, NULL);

  pkgbuild_path = calloc(1, PATH_MAX + 1);
  snprintf(pkgbuild_path, PATH_MAX, "%s/%s/PKGBUILD", dir, pkg);

  buffer = get_file_as_buffer(pkgbuild_path);
  if (! buffer) {
    if (config->color)
      cfprintf(stderr, "%<::%> Could not open PKGBUILD for dependency parsing.\n",
        config->colors->error);
    else
      fprintf(stderr, "!! Could not open PKGBUILD for dependency parsing.\n");
    return 1;
  }

  alpm_list_t **pkg_details[PKGDETAIL_MAX] = {
    &deplist, &deplist, NULL, NULL, NULL, NULL
  };

  pkgbuild_extinfo_get(&buffer, pkg_details, TRUE);
  free(buffer);

  if (!config->quiet && config->verbose >= 1) {
    if (config->color)
      cprintf("\n%<::%> Fetching uninstalled dependencies for %<%s%>...\n",
        config->colors->info, config->colors->pkg, pkg);
    else
      printf("\n:: Fetching uninstalled dependencies for %s...\n", pkg);
  }

  alpm_list_t *i = deplist;
  for (i = deplist; i; i = i->next) {
    char *depend = i->data;
    alpm_list_t *targ = NULL;

    if (config->verbose >= 2)
      printf("::DEBUG Attempting to find %s\n", depend);

    targ = alpm_list_add(targ, depend);
    alpm_list_t *results = alpm_db_search(db_local, targ);
    if (results) { /* installed */
      if (config->verbose >= 2)
        printf("::DEBUG %s is installed\n", depend);

      goto finish;
    }

    if (is_in_pacman(depend))
      goto finish;

    /* can't find it in pacman, check the AUR */
    results = query_aur_rpc(AUR_QUERY_TYPE_INFO, depend);
    if (results) {

      if (config->verbose >= 2)
        printf("::DEBUG %s is in the AUR\n", depend);

      ret++;
      download_taurball(results->data);

      aur_pkg_free(results->data);
      goto finish;
    }

    /* can't find it anywhere -- warn about this */
    if (config->color)
      cfprintf(stderr, "%<::%> Unresolvable dependency: '%s'\n",
        config->colors->warn, depend);
     else
      fprintf(stderr, "^^ Unresolvable dependency: '%s'\n", depend);


    finish:
      alpm_list_free(results);
      alpm_list_free(targ);
  }

  /* Cleanup */
  FREELIST(deplist);
  FREE(dir);
  FREE(pkgbuild_path);

  return ret;
}

/* vim: set ts=2 sw=2 et: */

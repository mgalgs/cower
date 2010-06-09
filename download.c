/*
 *  download.c
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

#include <errno.h>
#include <linux/limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "aur.h"
#include "curl.h"
#include "conf.h"
#include "pkgbuild.h"
#include "download.h"
#include "query.h"
#include "util.h"

int download_taurball(struct aur_pkg_t *aurpkg) {
  const char *filename, *pkgname;
  char *dir, *escaped;
  char fullpath[PATH_MAX + 1], url[AUR_URL_SIZE + 1];
  CURLcode curlstat;
  long httpcode;
  int result = 0;

  /* establish download dir */
  dir = calloc(1, PATH_MAX + 1);
  if (! config->download_dir) /* use pwd */
    dir = getcwd(dir, PATH_MAX);
  else
    dir = realpath(config->download_dir, dir);

  if (! dir) {
    if (config->color)
      cfprintf(stderr, "%<::%> specified path does not exist.\n", config->colors->error);
    else
      fprintf(stderr, "!! specified path does not exist.\n");

    return 1;
  }

  pkgname = aurpkg->name;
  filename = strrchr(aurpkg->urlpath, '/') + 1;

  snprintf(fullpath, PATH_MAX, "%s/%s", dir, filename);

  /* temporarily mask extension to check for the exploded dir existing */
  if (STREQ(fullpath + strlen(fullpath) - 7, ".tar.gz"))
    fullpath[strlen(fullpath) - 7] = '\0';

  if (file_exists(fullpath) && ! config->force) {
    if (config->color)
      cfprintf(stderr, "%<::%>", config->colors->error);
    else
      fprintf(stderr, "!!");

    fprintf(stderr, " %s already exists.\n   Use -f to force this operation.\n",
      fullpath);

    FREE(dir);
    return 1;
  }

  fullpath[strlen(fullpath)] = '.'; /* unmask extension */

  /* check for write access */
  FILE *fd = fopen(fullpath, "w");
  if (! fd) {
    result = errno;
    if (config->color)
      cfprintf(stderr, "%<::%> could not write to %s: ", config->colors->error, dir);
    else
      fprintf(stderr, "!! could not write to %s: ", dir);
    errno = result;
    perror("");

    FREE(dir);
    return result;
  }

  /* all clear to download */
  escaped = curl_easy_escape(curl, pkgname, strlen(pkgname));
  snprintf(url, AUR_URL_SIZE, AUR_PKG_URL, pkgname, pkgname);

  if (config->verbose >= 2)
    fprintf(stderr, "::DEBUG Fetching URL %s\n", url);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);

  curl_free(escaped);

  curlstat = curl_easy_perform(curl);
  fclose(fd);

  if (curlstat != CURLE_OK) {
    fprintf(stderr, "!! curl: %s\n", curl_easy_strerror(curlstat));
    result = curlstat;

  } else { /* curl reported no error */
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
    if (httpcode != 200) {
      fprintf(stderr, "!! curl: server responded with code %ld\n", httpcode);
      result = httpcode;

    } else { /* http response is kosher */
      if (config->color) {
        cprintf("%<::%> %<%s%> downloaded to %<%s%>\n",
          config->colors->info,
          config->colors->pkg, pkgname,
          config->colors->uptodate, dir);
      } else
        printf(":: %s downloaded to %s\n", pkgname, dir);

      /* fork bsdtar and extract the downloaded taurball */
      pid_t pid = fork();
      if (pid == 0) {
        if (config->verbose >= 2)
          fprintf(stderr, "::DEBUG bsdtar -C %s -xf %s\n", dir, fullpath);

        result = execlp("bsdtar", "bsdtar", "-C", dir, "-xf", fullpath, NULL);

      } else /* wait for bsdtar to finish */
        while (! waitpid(pid, NULL, WNOHANG));

      if (result == 0) /* no errors, delete the tarball */
        unlink(fullpath);
    }
  }

  FREE(dir);
  return result;
}


int cower_do_download(alpm_list_t *targets) {
  alpm_list_t *i;
  int ret = 0, dl_res;

  alpm_quick_init();

  for (i = targets; i; i = i->next) {
    if (is_in_pacman(i->data)) /* Skip it */
      continue;

    alpm_list_t *results = query_aur_rpc(AUR_QUERY_TYPE_INFO, i->data);
    if (results) { /* Found it in the AUR */
      dl_res = download_taurball(results->data);

      /* If the download didn't go smoothly, it's not ok to get depends */
      if (dl_res == 0 && config->getdeps)
        get_pkg_dependencies(i->data);

      aur_pkg_free(results->data);
      alpm_list_free(results);
    } else { /* Not found anywhere */
      if (config->color)
        cfprintf(stderr, "%<%s%>", config->colors->error, "::");
      else
        fprintf(stderr, "!!");
      fprintf(stderr, " no results for \"%s\"\n", (const char*)i->data);

      ret = 1;
    }
  }

  return ret;
}

/* vim: set ts=2 sw=2 et: */

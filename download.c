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
#include "depends.h"
#include "download.h"
#include "search.h"
#include "util.h"

/** 
* @brief download a taurball described by a JSON
* 
* @param root   the json describing the taurball
* 
* @return       0 on success, 1 on failure
*/
int aur_get_tarball(struct aur_pkg_t *aurpkg) {

  FILE *fd;
  const char *filename, *pkgname;
  char *dir;
  char fullpath[PATH_MAX + 1], url[AUR_URL_SIZE + 1];
  CURLcode curlstat;
  long httpcode;
  int result = 0;

  dir = calloc(1, PATH_MAX + 1);

  if (config->download_dir == NULL) /* use pwd */
    dir = getcwd(dir, PATH_MAX);
  else
    dir = realpath(config->download_dir, dir);

  if (! dir) {
    if (config->color) {
      cfprintf(stderr, "%<error:%> specified path does not exist.\n", RED);
    } else {
      fprintf(stderr, "error: specified path does not exist.\n");
    }
    return 1;
  }

  pkgname = aurpkg->name;

  snprintf(url, AUR_URL_SIZE, AUR_PKG_URL, pkgname, pkgname);

  if (config->verbose >= 2)
    fprintf(stderr, "::DEBUG Fetching URL %s\n", url);

  filename = strrchr(url, '/') + 1;

  snprintf(fullpath, PATH_MAX, "%s/%s", dir, filename);

  /* Mask extension to check for the exploded dir existing */
  if (STREQ(fullpath + strlen(fullpath) - 7, ".tar.gz"))
    fullpath[strlen(fullpath) - 7] = '\0';

  if (file_exists(fullpath) && ! config->force) {
    if (config->color)
      cfprintf(stderr, "%<%s%>", RED, "error:");
    else
      fprintf(stderr, "error:");

    fprintf(stderr, " %s already exists.\nUse -f to force this operation.\n",
      fullpath);

    result = 1;
  } else {
    fullpath[strlen(fullpath)] = '.'; /* Unmask extension */
    fd = fopen(fullpath, "w");
    if (fd != NULL) {
      curl_easy_setopt(curl, CURLOPT_URL, url);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);

      curlstat = curl_easy_perform(curl);
      if (curlstat != CURLE_OK) {
        fprintf(stderr, "curl: %s\n", curl_easy_strerror(curlstat));
        result = curlstat;
        goto cleanup;
      }

      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
      if (httpcode != 200) {
        fprintf(stderr, "curl: error: server responded with code %ld\n", httpcode);
        goto cleanup;
      }

      if (config->color)
        cprintf("%<%s%> downloaded to %<%s%>\n", WHITE, pkgname, GREEN, dir);
      else
        printf("%s downloaded to %s\n", pkgname, dir);

      fclose(fd);

      /* Fork off bsdtar to extract the taurball */
      pid_t pid; pid = fork();
      if (pid == 0) { /* Child process */
        if (config->verbose >= 2)
          fprintf(stderr, "::DEBUG bsdtar -C %s -xf %s\n", dir, fullpath);
        result = execlp("bsdtar", "bsdtar", "-C", dir, "-xf", fullpath, NULL);
      } else /* Back in the parent, waiting for child to finish */
        while (! waitpid(pid, NULL, WNOHANG));
        if (! result) /* If we get here, bsdtar finished successfully */
          unlink(fullpath);
    } else {
      result = errno;
      if (config->color) {
        cfprintf(stderr, "%<error:%> could not write to %s: ", RED, dir);
      } else {
        fprintf(stderr, "error: could not write to %s: ", dir);
      }
      errno = result;
      perror("");
    }
  }

cleanup:
  FREE(dir);
  return result;
}


int cower_do_download(alpm_list_t *targets) {
  alpm_list_t *i;
  int ret;

  alpm_quick_init();

  for (i = targets; i; i = alpm_list_next(i)) {

    if (is_in_pacman((const char*)i->data)) /* Skip it */
      continue;

    alpm_list_t *results = aur_rpc_query(AUR_QUERY_TYPE_INFO, i->data);
    if (results) { /* Found it in the AUR */
      ret += aur_get_tarball(results->data);
      if (config->getdeps)
        get_pkg_dependencies((const char*)i->data);
      aur_pkg_free(results->data);
      alpm_list_free(results);
    } else { /* Not found anywhere */
      if (config->color) {
        cfprintf(stderr, "%<%s%>", RED, "error:");
      } else {
        fprintf(stderr, "error:");
      }
      fprintf(stderr, " no results for \"%s\"\n", (const char*)i->data);
    }
  }

  return 0;
}

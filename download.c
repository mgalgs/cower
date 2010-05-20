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

#include <linux/limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "aur.h"
#include "conf.h"
#include "depends.h"
#include "download.h"
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
  const char *dir, *filename, *pkgname;
  char fullpath[PATH_MAX + 1], url[AUR_URL_SIZE + 1];
  int result = 0;

  if (config->download_dir == NULL) /* Use pwd */
    dir = getcwd(NULL, 0);
  else
    dir = realpath(config->download_dir, NULL);

  if (! dir || ! file_exists(dir)) {
    if (config->color) {
      cfprintf(stderr, "%<error:%> specified path does not exist.\n", RED);
    } else {
      fprintf(stderr, "error: specified path does not exist.\n");
    }
    return 1;
  }

  pkgname = aurpkg->name;

  if (config->verbose >= 2)
    fprintf(stderr, "::DEBUG Downloading Package: %s\n", pkgname);

  /* Build URL. Need this to get the taurball, durp */
  snprintf(url, AUR_URL_SIZE, AUR_PKG_URL, pkgname, pkgname);

  if (config->verbose >= 2)
    fprintf(stderr, "::DEBUG Using URL %s\n", url);

  /* Pointer to the 'basename' of the URL Path */
  filename = strrchr(url, '/') + 1;

  snprintf(fullpath, PATH_MAX, "%s/%s", dir, filename);

  /* Mask extension to check for the exploded dir existing */
  if (strcmp(strrchr(fullpath, '.'), ".gz") == 0) 
    fullpath[strlen(fullpath) - 7] = '\0';
  else if (strcmp(strrchr(fullpath, '.'), ".tgz") == 0)
    fullpath[strlen(fullpath) - 4] = '\0';

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
      curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_URL, url);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
      result = curl_easy_perform(curl);

      curl_easy_cleanup(curl);

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
      if (config->color) {
        cfprintf(stderr, "%<error:%> could not write to path %s\n", RED, dir);
      } else {
        fprintf(stderr, "error: could not write to path: %s\n", dir);
      }
      result = 1;
    }
  }

  FREE(dir);

  return result;
}


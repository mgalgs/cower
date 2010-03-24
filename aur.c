/*
 *  aur.c
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

/* Standard */
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Non-standard */
#include <alpm.h>
#include <curl/curl.h>
#include <jansson.h>

/* Local */
#include "aur.h"
#include "conf.h"
#include "fetch.h"
#include "util.h"

/** 
* @brief search for updates to foreign packages
* 
* @param foreign  an alpm_list_t of foreign pacman packages
* 
* @return number of updates available
*/
int aur_find_updates(alpm_list_t *foreign) {

  alpm_list_t *i;
  int ret = 0;

  /* Iterate over foreign packages */
  for (i = foreign; i; i = alpm_list_next(i)) {
    pmpkg_t *pmpkg = alpm_list_getdata(i);

    /* Do I exist in the AUR? */
    json_t *infojson = aur_rpc_query(AUR_RPC_QUERY_TYPE_INFO,
      alpm_pkg_get_name(pmpkg));

    if (infojson == NULL) { /* Not found, next candidate */
      json_decref(infojson);
      continue;
    }

    json_t *pkg;
    const char *remote_ver, *local_ver;

    pkg = json_object_get(infojson, "results");
    remote_ver = json_string_value(json_object_get(pkg, "Version"));
    local_ver = alpm_pkg_get_version(pmpkg);

    /* Version check */
    if (alpm_pkg_vercmp(remote_ver, local_ver) <= 0) {
      json_decref(infojson);
      continue;
    }

    ret++; /* Found an update, increment */
    if (config->op & OP_DL) /* -d found with -u */
      aur_get_tarball(infojson);
    else {
      if (config->color) {
        cprintf("%<%s%>\n", WHITE, alpm_pkg_get_name(pmpkg));
        if (! config->quiet)
          cprintf(" %<%s%> -> %<%s%>\n", GREEN, local_ver, GREEN, remote_ver);
      } else {
        if (! config->quiet)
          printf("%s %s -> %s\n", alpm_pkg_get_name(pmpkg), local_ver, remote_ver);
        else
          printf("%s\n", alpm_pkg_get_name(pmpkg));
      }
    }
  }

  return ret;
}

int aur_get_tarball(json_t *root) {

  CURL *curl;
  FILE *fd;
  const char *dir, *filename, *pkgname;
  char fullpath[PATH_MAX], url[128];
  int result = 0;
  json_t *pkginfo;

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

  /* Point to the juicy bits of the JSON */
  pkginfo = json_object_get(root, "results");
  pkgname = json_string_value(json_object_get(pkginfo, "Name"));

  /* Build URL. Need this to get the taurball, durp */
  snprintf(url, 128, AUR_PKG_URL,
    json_string_value(json_object_get(pkginfo, "URLPath")));

  /* Pointer to the 'basename' of the URL Path */
  filename = strrchr(url, '/') + 1;

  snprintf(fullpath, PATH_MAX, "%s/%s", dir, filename);

  /* Mask .tar.gz extension to check for the exploded dir existing */
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
      curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_URL, url);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
      result = curl_easy_perform(curl);

      curl_easy_cleanup(curl);
      curl_global_cleanup();

      if (config->color)
        cprintf("%<%s%> downloaded to %<%s%>\n", WHITE, pkgname, GREEN, dir);
      else
        printf("%s downloaded to %s\n", pkgname, dir);

      fclose(fd);

      /* Fork off bsdtar to extract the taurball */
      pid_t pid; pid = fork();
      if (pid == 0) { /* Child process */
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

  free((void*)dir);

  return result;
}

json_t *aur_rpc_query(int type, const char* arg) {

  char *text;
  char url[AUR_RPC_URL_SIZE];
  json_t *root, *return_type;
  json_error_t error;

  /* Format URL to pass to curl */
  snprintf(url, AUR_RPC_URL_SIZE, AUR_RPC_URL,
    type == AUR_RPC_QUERY_TYPE_INFO ? "info" : "search", arg);

  text = curl_get_json(url);
  if(!text)
    return NULL;

  /* Fetch JSON */
  root = json_loads(text, &error);
  free(text);

  /* Check for error */
  if(!root) {
    if (config->color) {
      cfprintf(stderr, "%<error:%> could not create JSON. Please report this error.\n",
        RED);
    } else {
      fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
    }
    return NULL;
  }

  /* Check return type in JSON */
  return_type = json_object_get(root, "type");
  if (! strcmp(json_string_value(return_type), "error")) {
    json_decref(root);
    return NULL;
  }

  return root; /* This needs to be freed in the calling function */
}


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

#include <alpm.h>
#include <jansson.h>

#include "conf.h"
#include "download.h"
#include "util.h"

/** 
* @brief write CURL response to a struct
* 
* @param ptr      pointer to the data retrieved by CURL
* @param size     size of each element
* @param nmemb    number of elements in the stream
* @param stream   data structure to append to
* 
* @return number of bytes written
*/
static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {

  struct write_result *result = (struct write_result *)stream;

  if(result->pos + size * nmemb >= JSON_BUFFER_SIZE - 1) {
    if (config->color) {
      cfprintf(stderr, "%<curl error:%> buffer too small.\n", RED);
    } else {
      fprintf(stderr, "curl error: buffer too small.\n");
    }
    return 0;
  }

  memcpy(result->data + result->pos, ptr, size * nmemb);
  result->pos += size * nmemb;

  return size * nmemb;
}

/** 
* @brief download a taurball described by a JSON
* 
* @param root   the json describing the taurball
* 
* @return       0 on success, 1 on failure
*/
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

  FREE(dir);

  return result;
}


/** 
* @brief fetch a JSON from the AUR via it's RPC interface
* 
* @param url      URL to fetch
* 
* @return string representation of the JSON
*/
char *curl_get_json(const char *url) {

  CURLcode status;
  char *data;
  long code;

  data = malloc(JSON_BUFFER_SIZE);
  if(!curl || !data) {
    if (config->color) {
      cfprintf(stderr, "%<error:%> could not allocate %d bytes.\n",
        RED, JSON_BUFFER_SIZE);
    } else {
      fprintf(stderr, "error: could not allocate %d bytes.\n",
        JSON_BUFFER_SIZE);
    }
    return NULL;
  }

  struct write_result write_result = {
    .data = data,
    .pos = 0
  };

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

  status = curl_easy_perform(curl);
  if(status != 0) {
    if (config->color) {
      cfprintf(stderr, "%<curl error:%> unable to request data from %s\n", RED, url);
    } else {
      fprintf(stderr, "curl error: unable to request data from %s\n", url);
    }
    fprintf(stderr, "%s\n", curl_easy_strerror(status));
    free(data);
    return NULL;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  if(code != 200) {
    if (config->color) {
      cfprintf(stderr, "%<curl error:%> server responded with code %l", RED, code);
    } else {
      fprintf(stderr, "curl error: server responded with code %ld\n", code);
    }
    free(data);
    return NULL;
  }

  /* zero-terminate the result */
  data[write_result.pos] = '\0';

  return data;
}

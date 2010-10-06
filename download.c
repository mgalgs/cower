/* Copyright (c) 2010 Dave Reisner
 *
 * download.c
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
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

#include "aur.h"
#include "curl.h"
#include "conf.h"
#include "pkgbuild.h"
#include "download.h"
#include "query.h"
#include "util.h"

static int archive_extract_tar_gz(FILE *fd) {
  struct archive *archive = archive_read_new();
  struct archive_entry *entry;
  const int archive_flags = ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_TIME;

  archive_read_support_compression_all(archive);
  archive_read_support_format_all(archive);

  if (archive_read_open_FILE(archive, fd) != ARCHIVE_OK)
    return 1;

  while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
    switch (archive_read_extract(archive, entry, archive_flags)) {
      case ARCHIVE_OK:
      case ARCHIVE_WARN:
      case ARCHIVE_RETRY: break;
      case ARCHIVE_EOF:
      case ARCHIVE_FATAL: return 1;
    }
  }

  archive_read_close(archive);
  archive_read_finish(archive);

  return 0;
}

int download_taurball(struct aur_pkg_t *aurpkg) {
  const char *filename, *pkgname;
  char *dir, *escaped, *fullpath, *url;
  CURLcode curlstat;
  long httpcode;
  int result = 0;

  /* establish download dir */
  if (! config->download_dir) /* use pwd */
    dir = getcwd(NULL, PATH_MAX);
  else /* resolve specified dir */
    dir = realpath(config->download_dir, NULL);

  if (! dir) {
    if (config->color)
      cfprintf(stderr, "%<::%> specified path does not exist.\n", config->colors->error);
    else
      fprintf(stderr, "!! specified path does not exist.\n");

    return 1;
  }

  pkgname = aurpkg->name;
  filename = basename(aurpkg->urlpath);

  if (asprintf(&fullpath, "%s/%s", dir, filename) < 0)
    return (-ENOMEM);

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

    FREE(fullpath);
    FREE(dir);
    return 1;
  }

  fullpath[strlen(fullpath)] = '.'; /* unmask extension */

  /* check for write access */
  FILE *fd = fopen(fullpath, "w+");
  if (! fd) {
    result = errno;
    if (config->color)
      cfprintf(stderr, "%<::%> could not write to %s: ", config->colors->error, dir);
    else
      fprintf(stderr, "!! could not write to %s: ", dir);
    errno = result;
    perror("");

    FREE(dir);
    FREE(fullpath);
    return result;
  }

  /* all clear to download */
  escaped = curl_easy_escape(curl, pkgname, strlen(pkgname));
  if (asprintf(&url, AUR_PKG_URL, escaped, escaped) < 0)
    return -ENOMEM;

  curl_free(escaped);

  if (config->verbose >= 2)
    fprintf(stderr, "::DEBUG Fetching URL %s\n", url);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);

  curlstat = curl_easy_perform(curl);

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

      if (config->download_dir)
        if (chdir(dir) != 0)
          return errno;

      rewind(fd);
      if (archive_extract_tar_gz(fd) == 0) /* no errors, delete the tarball */
        unlink(fullpath);
      else {
        if (config->color) {
          cfprintf(stderr, "%<::%> failed to extract %s\n", config->colors->error, fullpath);
        } else
          printf("!! failed to extract %s\n", fullpath);
      }
    }
  }

  fclose(fd);

  FREE(fullpath);
  FREE(url);
  FREE(dir);

  return result;
}

int cower_do_download(alpm_list_t *targets) {
  alpm_list_t *i;
  int ret = 0;

  alpm_quick_init();

  for (i = targets; i; i = i->next) {
    if (is_in_pacman(i->data)) /* Skip it */
      continue;

    alpm_list_t *results = query_aur_rpc(AUR_QUERY_TYPE_INFO, i->data);
    if (results) { /* Found it in the AUR */

      /* If the download didn't go smoothly, it's not ok to get depends */
      if (!download_taurball(results->data) && config->getdeps)
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

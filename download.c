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
#include "util.h"
#include "yajl.h"

size_t archive_extract_stream(void *ptr, size_t size, size_t nmemb, void *userdata) {
  struct archive *archive = (struct archive*)userdata;
  struct archive_entry *entry;

  const int archive_flags = ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_TIME;

  if (archive_read_open_memory(archive, ptr, size *nmemb) != ARCHIVE_OK) {
    return(0);
  }

  while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
    switch (archive_read_extract(archive, entry, archive_flags)) {
      case ARCHIVE_OK:
      case ARCHIVE_WARN:
      case ARCHIVE_RETRY: break;
      case ARCHIVE_EOF:
      case ARCHIVE_FATAL: return(0);
    }
  }

  archive_read_close(archive);

  return(size * nmemb);
}

int download_taurball(struct aur_pkg_t *aurpkg) {
  const char *pkgname;
  char *dir, *escaped, *fullpath, *url;
  CURL *curl;
  CURLcode curlstat;
  long httpcode;
  int result = 0;
  struct archive *archive;

  /* establish download dir */
  if (!config->download_dir) { /* use pwd */
    dir = getcwd(NULL, PATH_MAX);
  } else { /* resolve specified dir */
    dir = realpath(config->download_dir, NULL);
  }

  if (!dir) {
    cwr_fprintf(stderr, LOG_ERROR, "specified path does not exist.\n");
    return(1);
  }

  pkgname = aurpkg->name;

  cwr_asprintf(&fullpath, "%s/%s", dir, pkgname);

  if (file_exists(fullpath) && !config->force) {
    cwr_fprintf(stderr, LOG_ERROR, "`%s' already exists.\n"
        "Use -f to force this operation.\n", fullpath);
    FREE(fullpath);
    FREE(dir);
    return(1);
  }

  /* all clear to download */
  curl = curl_local_init();
  escaped = curl_easy_escape(curl, pkgname, strlen(pkgname));
  cwr_asprintf(&url, AUR_PKG_URL, escaped, escaped);
  curl_free(escaped);

  cwr_printf(LOG_DEBUG, "Fetching URL %s\n", url);

  if (access(dir, W_OK)) {
    cwr_fprintf(stderr, LOG_ERROR, "could not write to %s\n", dir);
    FREE(dir);
    FREE(fullpath);
    return(1);
  }

  archive = archive_read_new();
  archive_read_support_compression_all(archive);
  archive_read_support_format_all(archive);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, archive);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, archive_extract_stream);

  if (config->download_dir) {
    chdir(dir);
  }

  curlstat = curl_easy_perform(curl);
  if (curlstat != CURLE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: %s\n", curl_easy_strerror(curlstat));
    result = 1;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode != 200) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: server responded with code %ld\n",
        httpcode);
    result = 1;
  }

  archive_read_finish(archive);

  if (result == 0) { /* still no error, we have success */
    cwr_printf(LOG_INFO, "%s%s%s downloaded to %s%s%s\n",
        config->strings->pkg, pkgname, config->strings->c_off,
        config->strings->uptodate, dir, config->strings->c_off);
  }

  curl_easy_cleanup(curl);
  FREE(fullpath);
  FREE(url);
  FREE(dir);

  return(result);
}

int cower_do_download(alpm_list_t *targets) {
  alpm_list_t *i;
  int ret = 0;

  alpm_quick_init();

  for (i = targets; i; i = i->next) {
    if (alpm_provides_pkg(i->data)) { /* Skip it */
      continue;
    }

    alpm_list_t *results = aur_fetch_json(AUR_QUERY_TYPE_INFO, i->data);
    if (results) { /* Found it in the AUR */

      /* If the download didn't go smoothly, it's not ok to get depends */
      if (!download_taurball(results->data) && config->getdeps) {
        get_pkg_dependencies(i->data);
      }

      aur_pkg_free(results->data);
      alpm_list_free(results);
    } else { /* Not found anywhere */
      cwr_fprintf(stderr, LOG_ERROR, "no results for \"%s\"\n",
          (const char*)i->data); 
      ret = 1;
    }
  }

  return(ret);
}

/* vim: set ts=2 sw=2 et: */

/* Copyright (c) 2010 Dave Reisner
 *
 * update.c
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

#include <stdio.h>
#include <pthread.h>

#include "aur.h"
#include "conf.h"
#include "download.h"
#include "pacman.h"
#include "yajl.h"
#include "util.h"

void *thread_main(void *arg) {

  pmpkg_t *pmpkg = arg;

  cwr_printf(LOG_DEBUG, "Checking %s%s%s for updates...\n",
      config->strings->pkg, alpm_pkg_get_name(pmpkg), config->strings->c_off);

  /* Do I exist in the AUR? */
  alpm_list_t *results = aur_fetch_json(AUR_QUERY_TYPE_INFO, alpm_pkg_get_name(pmpkg));

  if (!results) { /* Not found, next candidate */
    return(NULL);
  }

  const char *remote_ver, *local_ver;
  struct aur_pkg_t *aurpkg;

  aurpkg = (struct aur_pkg_t*)results->data;
  remote_ver = aurpkg->ver;
  local_ver = alpm_pkg_get_version(pmpkg);

  /* Version check */
  if (alpm_pkg_vercmp(remote_ver, local_ver) > 0) {

    /* download if -d passed with -u but not ignored */
    if ((config->op & OP_DL)) {
      if (alpm_list_find_str(config->ignorepkgs, aurpkg->name) != NULL) {
        cwr_fprintf(stderr, LOG_WARN, " ignoring package %s\n", aurpkg->name);
      } else {
        download_taurball(aurpkg);
      }
    } else {
      print_pkg_update(aurpkg->name, local_ver, remote_ver);
    }
  }

  aur_pkg_free(aurpkg);
  alpm_list_free(results);

  return(NULL);
}

int cower_do_update() {
  int ret, n, thread_count;
  pthread_t *threads;
  pthread_attr_t attr;

  alpm_quick_init();
  ret = 0;

  alpm_list_t *foreign_pkgs = alpm_query_foreign();
  if (!foreign_pkgs) {
    return(ret);
  }

  thread_count = alpm_list_count(foreign_pkgs);
  threads = calloc(thread_count, sizeof *threads);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  /* Iterate over targets packages */
  alpm_list_t *i;
  for (i = foreign_pkgs, n = 0; i; i = i->next, n++) {
    pthread_create(&threads[n], &attr, thread_main, i->data);
  }

  for (n = 0; n < thread_count; n++) {
    pthread_join(threads[n], NULL);
  }

  alpm_list_free(foreign_pkgs);
  free(threads);

  return(ret);
}

/* vim: set ts=2 sw=2 et: */

/* Copyright (c) 2010 Dave Reisner
 *
 * conf.c
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
#include <stdlib.h>

#include "aur.h"
#include "conf.h"
#include "util.h"

struct config_t *config = NULL; /* global config variable */

int config_free(struct config_t *oldconfig) {

  if (oldconfig == NULL) {
    return(-1);
  }

  /* free composited memory */
  if (oldconfig->color) {
    free(oldconfig->strings->repo);
    free(oldconfig->strings->pkg);
    free(oldconfig->strings->uptodate);
    free(oldconfig->strings->outofdate);
    free(oldconfig->strings->url);
    free(oldconfig->strings->info);
    free(oldconfig->strings->warn);
    free(oldconfig->strings->error);
    free(oldconfig->strings->c_off);
  }

  free(oldconfig->strings);

  free(oldconfig->colors);
  FREE(oldconfig->download_dir);
  FREELIST(oldconfig->ignorepkgs);

  FREE(oldconfig);

  return(0);
}

struct config_t *config_new(void) {
  struct config_t *newconfig = calloc(1, sizeof *newconfig);
  if(!newconfig) {
    cwr_fprintf(stderr, LOG_ERROR, "error allocating %zd bytes\n", sizeof *newconfig);
    return(NULL);
  }

  newconfig->colors = calloc(1, sizeof *(newconfig->colors));
  newconfig->strings = calloc(1, sizeof *(newconfig->strings));

  /* default options */
  newconfig->op = newconfig->color = newconfig->getdeps = 0;
  newconfig->force = newconfig->quiet = newconfig->moreinfo = 0;

  newconfig->logmask = LOG_ERROR|LOG_WARN|LOG_INFO;
  newconfig->download_dir = NULL;
  newconfig->ignorepkgs = NULL;
  newconfig->proto = HTTP;

  newconfig->colors->repo = BOLDMAGENTA;
  newconfig->colors->pkg = BOLDWHITE;
  newconfig->colors->uptodate = BOLDGREEN;
  newconfig->colors->outofdate = BOLDRED;
  newconfig->colors->url = BOLDCYAN;
  newconfig->colors->info = BOLDBLUE;
  newconfig->colors->warn = BOLDYELLOW;
  newconfig->colors->error = BOLDRED;

  return(newconfig);
}

/* vim: set ts=2 sw=2 et: */

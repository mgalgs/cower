/* Copyright (c) 2010 Dave Reisner
 *
 * package.c
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

#include <stdlib.h>
#include <string.h>

#include "package.h"
#include "util.h"

int aur_pkg_cmp(const void *p1, const void *p2) {
  struct aur_pkg_t *pkg1 = (struct aur_pkg_t*)p1;
  struct aur_pkg_t *pkg2 = (struct aur_pkg_t*)p2;

  return strcmp((const char*)pkg1->name, (const char*)pkg2->name);
}

void aur_pkg_free(void *pkg) {
  if (!pkg)
    return;

  struct aur_pkg_t *it = (struct aur_pkg_t*)pkg;

  FREE(it->name);
  FREE(it->ver);
  FREE(it->desc);
  FREE(it->url);
  FREE(it->urlpath);
  FREE(it->lic);

  FREELIST(it->depends);
  FREELIST(it->makedepends);
  FREELIST(it->optdepends);
  FREELIST(it->provides);
  FREELIST(it->conflicts);
  FREELIST(it->replaces);

  FREE(it);
}

struct aur_pkg_t *aur_pkg_new() {
  struct aur_pkg_t *pkg;

  pkg = calloc(1, sizeof *pkg);

  pkg->name = pkg->ver = pkg->desc = pkg->lic = pkg->url = pkg->urlpath = NULL;
  pkg->depends = pkg->makedepends = pkg->optdepends = NULL;
  pkg->id = pkg->cat = pkg->ood = pkg->votes = 0;

  return pkg;
}

/* vim: set ts=2 sw=2 et: */

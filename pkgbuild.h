/* Copyright (c) 2010 Dave Reisner
 *
 * pkgbuild.h
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

#ifndef _COWER_PKGBUILD_H
#define _COWER_PKGBUILD_H

#include "package.h"
#include "pacman.h"

#define PKGBUILD_DEPENDS      "depends=("
#define PKGBUILD_MAKEDEPENDS  "makedepends=("
#define PKGBUILD_OPTDEPENDS   "optdepends=("
#define PKGBUILD_PROVIDES     "provides=("
#define PKGBUILD_CONFLICTS    "conflicts=("
#define PKGBUILD_REPLACES     "replaces=("

enum {
  PKGDETAIL_DEPENDS = 0,
  PKGDETAIL_MAKEDEPENDS,
  PKGDETAIL_OPTDEPENDS,
  PKGDETAIL_PROVIDES,
  PKGDETAIL_CONFLICTS,
  PKGDETAIL_REPLACES,
  PKGDETAIL_MAX /* sigil - must be last */
};

int get_pkg_dependencies(const char*);
alpm_list_t *pkgbuild_get_deps(char**);
void pkgbuild_extinfo_get(char**, alpm_list_t**[], int);

#endif /* _COWER_PKGBUILD_H */

/* vim: set ts=2 sw=2 et: */

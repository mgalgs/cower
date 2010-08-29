/* Copyright (c) 2010 Dave Reisner
 *
 * package.h
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

#ifndef _COWER_PACKAGE_H
#define _COWER_PACKAGE_H

#include <alpm_list.h>

#define PKG_OUT_REPO          "Repository"
#define PKG_OUT_NAME          "Name"
#define PKG_OUT_VERSION       "Version"
#define PKG_OUT_URL           "URL"
#define PKG_OUT_AURPAGE       "AUR Page"
#define PKG_OUT_PROVIDES      "Provides"
#define PKG_OUT_DEPENDS       "Depends"
#define PKG_OUT_MAKEDEPENDS   "Makedepends"
#define PKG_OUT_CONFLICTS     "Conflicts"
#define PKG_OUT_REPLACES      "Replaces"
#define PKG_OUT_CAT           "Category"
#define PKG_OUT_NUMVOTES      "Number of Votes"
#define PKG_OUT_LICENSE       "License"
#define PKG_OUT_OOD           "Out of Date"
#define PKG_OUT_DESC          "Description"

struct aur_pkg_t {
  int id;
  const char *name;
  const char *ver;
  int cat;
  const char *desc;
  const char *url;
  const char *urlpath;
  const char *lic;
  int votes;
  int ood;

  /* Detailed info */
  alpm_list_t *depends;
  alpm_list_t *makedepends;
  alpm_list_t *optdepends;
  alpm_list_t *provides;
  alpm_list_t *conflicts;
  alpm_list_t *replaces;
};

int aur_pkg_cmp(const void*, const void*);
void aur_pkg_free(void*);
struct aur_pkg_t *aur_pkg_new(void);

#endif /* _COWER_PACKAGE_H */

/* vim: set ts=2 sw=2 et: */

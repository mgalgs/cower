/* Copyright (c) 2010 Dave Reisner
 *
 * pacman.h
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

#ifndef _COWER_PACMAN_H
#define _COWER_PACMAN_H

#include <alpm.h>

#define PACCONF   "/etc/pacman.conf"

alpm_list_t *alpm_list_mmerge_dedupe(alpm_list_t*, alpm_list_t*, alpm_list_fn_cmp, alpm_list_fn_free);
alpm_list_t *alpm_list_remove_item(alpm_list_t*, alpm_list_t*, alpm_list_fn_free);
alpm_list_t *alpm_query_foreign(void);
void alpm_quick_init(void);
int alpm_provides_pkg(const char*);

pmdb_t *db_local;
extern pmdb_t *db_local;

#endif /* _COWER_PACMAN_H */

/* vim: set ts=2 sw=2 et: */

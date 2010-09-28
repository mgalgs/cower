/* Copyright (c) 2010 Dave Reisner
 *
 * aur.h
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

#ifndef _COWER_AUR_H
#define _COWER_AUR_H

/* Various URLs used to interact with the AUR */
#define AUR_PKGBUILD_PATH     "http://aur.archlinux.org/packages/%s/%s/PKGBUILD"
#define AUR_PKG_URL           "http://aur.archlinux.org/packages/%s/%s.tar.gz"
#define AUR_RPC_URL           "http://aur.archlinux.org/rpc.php?type=%s&arg=%s"
#define AUR_PKG_URL_FORMAT    "http://aur.archlinux.org/packages.php?ID="

#define AUR_QUERY_TYPE        "type"
#define AUR_QUERY_TYPE_INFO   "info"
#define AUR_QUERY_TYPE_SEARCH "search"
#define AUR_QUERY_ERROR       "error"

/* JSON key names */
#define AUR_ID                "ID"
#define AUR_NAME              "Name"
#define AUR_VER               "Version"
#define AUR_CAT               "CategoryID"
#define AUR_DESC              "Description"
#define AUR_LOC               "LocationID"
#define AUR_URL               "URL"
#define AUR_URLPATH           "URLPath"
#define AUR_LICENSE           "License"
#define AUR_VOTES             "NumVotes"
#define AUR_OOD               "OutOfDate"

#endif /* _COWER_AUR_H */

/* vim: set ts=2 sw=2 et: */

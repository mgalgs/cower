/*
 *  download.h
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

#ifndef _DOWNLOAD_H
#define _DOWNLOAD_H

#include "curl.h"
#include "package.h"
#include "pacman.h"

#define AUR_QUERY_TYPE_INFO   "info"
#define AUR_QUERY_TYPE_SEARCH "search"

int aur_get_tarball(struct aur_pkg_t*);
int cower_do_download(alpm_list_t*);

#endif /* _DOWNLOAD_H */

/*
 *  aur.h
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

#ifndef _AUR_H
#define _AUR_H

#define URL_FORMAT      "http://aur.archlinux.org/rpc.php?type=%s&arg=%s"
#define URL_SIZE        256

#define BUFFER_SIZE     (256 * 1024) /* 256kB */

/* Info */
struct aurpkg *aur_pkg_info(char*, int*);

/* Search */
void print_search_results(json_t*, int*);
struct json_t *aur_pkg_search(char*, int*);

/* Download */
int get_taurball(const char*, char*, int*);

#endif /* _AUR_H */

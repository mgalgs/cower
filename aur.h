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

#define AUR_PKG_URL         "http://aur.archlinux.org%s"
#define AUR_RPC_URL         "http://aur.archlinux.org/rpc.php?type=%s&arg=%s"
#define AUR_PKG_URL_FORMAT  "http://aur.archlinux.org/packages.php?ID=%s"

#define AUR_RPC_URL_SIZE    256
#define JSON_BUFFER_SIZE    (256 * 1024) /* 256kB */

#define AUR_RPC_QUERY_TYPE_INFO   1
#define AUR_RPC_QUERY_TYPE_SEARCH 2

/* Updates */
void aur_find_updates(alpm_list_t*);

/* Download */
int aur_get_tarball(json_t*, char*);

/* RPC Query */
json_t *aur_rpc_query(int, const char*);

#endif /* _AUR_H */

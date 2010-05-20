/*
 *  search.h
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

#ifndef _COWER_SEARCH_H
#define _COWER_SEARCH_H

#include <alpm.h>

alpm_list_t *aur_rpc_query(const char*, const char*);
alpm_list_t *cower_do_info(alpm_list_t*);
alpm_list_t *cower_do_search(alpm_list_t*);
int get_pkg_availability(alpm_list_t*);

#endif /* _COWER_SEARCH_H */

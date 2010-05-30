/*
 *  query.h
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

#ifndef _COWER_QUERY_H
#define _COWER_QUERY_H

#include "pacman.h"

alpm_list_t *query_aur_rpc(const char*, const char*);
alpm_list_t *cower_do_query(alpm_list_t*, const char*);

#endif /* _COWER_QUERY_H */

/* vim: set ts=2 sw=2 et: */

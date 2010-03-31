/*
 *  depends.c
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

#include <alpm.h>

int fetch_depends(alpm_list_t*);
int get_pkg_dependencies(const char*, const char*);
alpm_list_t *parse_bash_array(alpm_list_t*, char*);
alpm_list_t *parsedeps(const char*, alpm_list_t*);

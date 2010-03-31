/*
 *  util.h
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
#ifndef _UTIL_H
#define _UTIL_H

#include <alpm.h>
#include <jansson.h>

/* constants for cfprintf */
#define C_ON    "\033[0;3"
#define C_OFF     "\033[0;m"

#define FREE(p) do { free((void*)p); p = NULL; } while (0)
#define TRUE  1
#define FALSE 0

/* colors */
enum {
  BLACK   = 0,
  RED     = 1,
  GREEN   = 2,
  YELLOW  = 3,
  BLUE    = 4,
  MAGENTA = 5,
  CYAN    = 6,
  WHITE   = 7,
  FG      = 8
};

alpm_list_t *agg_search_results(alpm_list_t*, json_t*);
char *itoa(unsigned int, int);
int cfprintf(FILE*, const char*, ...);
int cprintf(const char*, ...);
int file_exists(const char*);
void print_pkg_info(json_t*);
void print_pkg_search(alpm_list_t*);
void print_pkg_update(const char*, const char*, const char*);
char *strtrim(char*);

#endif /* _UTIL_H */

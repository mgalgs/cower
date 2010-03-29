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

/* constants for cfprintf */
#define C_ON    "\033[1;3"
#define C_OFF     "\033[1;m"

#define FREE(p) do { free((void*)p); p = NULL; } while (0)
#define TRUE  1
#define FALSE 0

/* colors */
enum {
  BLACK   = 0,  BOLDBLACK   = 9,
  RED     = 1,  BOLDRED     = 10,
  GREEN   = 2,  BOLDGREEN   = 11,
  YELLOW  = 3,  BOLDYELLOW  = 12,
  BLUE    = 4,  BOLDBLUE    = 13,
  MAGENTA = 5,  BOLDMAGENTA = 14,
  CYAN    = 6,  BOLDCYAN    = 15,
  WHITE   = 7,  BOLDWHITE   = 16,
  FG      = 8,  BOLDFG      = 17
};

alpm_list_t *agg_search_results(alpm_list_t*, json_t*);
char *itoa(unsigned int, int);
int cfprintf(FILE*, const char*, ...);
int cprintf(const char*, ...);
int file_exists(const char*);
void print_pkg_info(json_t*);
void print_pkg_search(alpm_list_t*);

#endif /* _UTIL_H */

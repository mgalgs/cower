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
#ifndef _COWER_UTIL_H
#define _COWER_UTIL_H

#include <stdio.h>

#include "pacman.h"
#include "package.h"

/* constants for cfprintf */
#define C_ON      "\033[%d;3%dm"
#define C_OFF     "\033[1;0m"

#define STREQ(x,y)            (strcmp(x,y) == 0)
#define NOOP(x)               if(x){}
#define FREE(p)               do { free((void*)p); p = NULL; } while (0)
#define STR_STARTS_WITH(x,y)  (strncmp(x, y, strlen(y)) == 0)

#define TRUE                  1
#define FALSE                 0

#define INDENT                18

/* colors */
enum {
  BLACK = 0,  RED,
  GREEN,      YELLOW,
  BLUE,       MAGENTA,
  CYAN,       WHITE,
  FG,         BG,
  BOLDBLACK,  BOLDRED,
  BOLDGREEN,  BOLDYELLOW,
  BOLDBLUE,   BOLDMAGENTA,
  BOLDCYAN,   BOLDWHITE,
  BOLDFG,     BOLDBG,

  COLOR_MAX, /* sigil - must be last */
};

int cfprintf(FILE*, const char*, ...);
int cprintf(const char*, ...);
int file_exists(const char*);
off_t filesize(const char*);
char *get_file_as_buffer(const char*);
char *itoa(unsigned int, int);
void print_extinfo_list(const char*, alpm_list_t*, size_t, int);
void print_pkg_info(struct aur_pkg_t*);
void print_pkg_search(alpm_list_t*);
void print_pkg_update(const char*, const char*, const char*);
void print_wrapped(const char*, size_t, int);
char *ltrim(char*);
char *strtrim(char*);

#endif /* _COWER_UTIL_H */

/* vim: set ts=2 sw=2 et: */

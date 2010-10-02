/* Copyright (c) 2010 Dave Reisner
 *
 * util.h
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

#ifndef _COWER_UTIL_H
#define _COWER_UTIL_H

#include <stdio.h>
#include <alpm_list.h>

#include "pacman.h"
#include "package.h"

/* constants for cfprintf */
#define C_ON      "\033[%d;3%dm"
#define C_OFF     "\033[1;0m"

#define STREQ(x,y)            (strcmp((x),(y)) == 0)
#define NOOP(x)               if(x){}
#define FREE(p)               do { free((void*)p); p = NULL; } while (0)
#define STR_STARTS_WITH(x,y)  (strncmp((x),(y), strlen(y)) == 0)

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
alpm_list_t *strsplit(const char*, const char);
char *relative_to_absolute_path(const char*);

#endif /* _COWER_UTIL_H */

/* vim: set ts=2 sw=2 et: */

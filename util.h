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


/* Color constants. Add for bold */
#define BLACK   0
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define MAGENTA 5
#define CYAN    6
#define WHITE   7
#define FG      8
#define BOLD    9

/* Operations */
enum {
    OPER_SEARCH = 1,
    OPER_UPDATE = 2,
    OPER_INFO = 4,
    OPER_DOWNLOAD = 8
};

/* Options */
enum {
    OPT_COLOR = 1,
    OPT_VERBOSE = 2,
    OPT_FORCE = 4,
    OPT_QUIET = 8
};

char *colorize(const char*, int, char*);
int file_exists(const char*);

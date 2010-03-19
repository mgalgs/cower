/*
 *  util.c
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

#include <stdio.h>
#include <sys/stat.h>

#include "util.h"

char *colorize(const char* input, int color, char* buffer) {
    /* A buffer has to be passed to this function to avoid
     * memory loss. Regardless, it needs to return the buffer
     * so that it can be used in-place.
     */
    sprintf(buffer, "\033[1;3%dm%s\033[1;m", color, input);
    return buffer;
}

int file_exists(const char* fd) {
    struct stat st;
    if (! stat(fd, &st)) {
        return 1;
    } else {
        return 0;
    }
}


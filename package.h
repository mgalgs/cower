/*
 *  package.h
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

struct aurpkg {
    int ID;
    int CategoryID;
    int LocationID;
    int NumVotes;
    int OutOfDate;
    const char *Name;
    const char *Version;
    const char *Description;
    const char *URL;
    const char *URLPath;
    const char *License;
};

void get_pkg_details(json_t*, struct aurpkg**);
void print_package(struct aurpkg*);

extern int opt_mask;

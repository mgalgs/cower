/*
 *  depends.h
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

#ifndef _COWER_DEPENDS_H
#define _COWER_DEPENDS_H

#include "pacman.h"

#define PKGBUILD_DEPENDS      "depends=("
#define PKGBUILD_MAKEDEPENDS  "makedepends=("
#define PKGBUILD_OPTDEPENDS   "optdepends=("

int get_pkg_dependencies(const char*);

#endif /* _COWER_DEPENDS_H */

/* vim: set ts=2 sw=2 et: */

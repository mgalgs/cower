/*
 *  aur.h
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

#ifndef _COWER_AUR_H
#define _COWER_AUR_H

#define AUR_PKG_URL         "http://aur.archlinux.org/packages/%s/%s.tar.gz"
#define AUR_RPC_URL         "http://aur.archlinux.org/rpc.php?type=%s&arg=%s"
#define AUR_PKG_URL_FORMAT  "http://aur.archlinux.org/packages.php?ID="

#define AUR_URL_SIZE    256

#define AUR_ID        "ID"
#define AUR_NAME      "Name"
#define AUR_VER       "Version"
#define AUR_CAT       "CategoryID"
#define AUR_DESC      "Description"
#define AUR_LOC       "LocationID"
#define AUR_URL       "URL"
#define AUR_URLPATH   "URLPath"
#define AUR_LICENSE   "License"
#define AUR_VOTES     "NumVotes"
#define AUR_OOD       "OutOfDate"

#endif /* _COWER_AUR_H */

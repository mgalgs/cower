/*
 *  curl.h
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

#ifndef _COWER_CURL_H
#define _COWER_CURL_H

#include <curl/curl.h>
#include <curl/easy.h>

#define COWER_USERAGENT    "cower/2.x"

char *curl_get_text_file(const char*);
int curl_local_init(void);

CURL *curl;
extern CURL *curl;

#endif /* _COWER_CURL_H */

/* vim: set ts=2 sw=2 et: */

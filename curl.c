/*
 *  curl.c
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
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "curl.h"

int curl_local_init() {
  curl = curl_easy_init();

  if (! curl)
    return 1;

  if (config->verbose > 3) {
    printf("::DEBUG:: Initializing curl\n");
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
  }

  curl_easy_setopt(curl, CURLOPT_ENCODING, "deflate, gzip");

  return 0;
}


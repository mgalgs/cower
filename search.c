/*
 *  search.c
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

/* standard */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* non-standard */
#include <alpm.h>
#include <jansson.h>

/* local */
#include "alpmutil.h"
#include "download.h"
#include "conf.h"
#include "search.h"
#include "util.h"

/** 
* @brief send a query to the AUR's rpc interface
* 
* @param type   search or info
* @param arg    argument to query
* 
* @return       a JSON loaded with the results of the query
*/
json_t *aur_rpc_query(int type, const char* arg) {

  char *text;
  char url[AUR_RPC_URL_SIZE];
  json_t *root, *return_type;
  json_error_t error;

  /* Format URL to pass to curl */
  snprintf(url, AUR_RPC_URL_SIZE, AUR_RPC_URL,
    type == AUR_RPC_QUERY_TYPE_INFO ? "info" : "search", arg);

  text = curl_get_json(url);
  if(!text)
    return NULL;

  /* Fetch JSON */
  root = json_loads(text, &error);
  free(text);

  /* Check for error */
  if(!root) {
    if (config->color) {
      cfprintf(stderr, "%<error:%> could not create JSON. Please report this error.\n",
        RED);
    } else {
      fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
    }
    return NULL;
  }

  /* Check return type in JSON */
  return_type = json_object_get(root, "type");
  if (! strcmp(json_string_value(return_type), "error")) {
    json_decref(root);
    return NULL;
  }

  return root; /* This needs to be freed in the calling function */
}


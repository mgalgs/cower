/*
 *  conf.c
 *
 *  Copyright (c) 2006-2009 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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

#include "conf.h"
#include "util.h"

struct config_t *config = NULL; /* global config variable */

/** 
* @brief free a config struct
* 
* @param oldconfig  config to be freed
* 
* @return 0 on success, non-zero on failure
*/
int config_free(struct config_t *oldconfig) {

  if (oldconfig == NULL) {
    return -1;
  }

  /* free malloc'd strings within config */
  FREE(oldconfig->download_dir);
  free(oldconfig);
  oldconfig = NULL;

  return 0;
}

/** 
* @brief allocate a new config struct
* 
* @return the new config, or NULL on failure
*/
struct config_t *config_new(void) {
  struct config_t *newconfig = calloc(1, sizeof *newconfig);
  if(!newconfig) {
    fprintf(stderr, "error allocating %zd bytes\n", sizeof *newconfig);
      return(NULL);
  }

  /* default options */
  newconfig->op = 0;
  newconfig->download_dir = NULL;
  newconfig->color = 0;
  newconfig->getdeps = 0;
  newconfig->force = 0;
  newconfig->quiet = 0;
  newconfig->verbose = 0;

  return newconfig;
}

/* vim: set ts=2 sw=2 et: */

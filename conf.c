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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "conf.h"
#include "util.h"

/* global config variable */
config_t *config = NULL;

config_t *config_new(void) {
    config_t *newconfig = calloc(1, sizeof(config_t));
    if(!newconfig) {
        fprintf(stderr, "error allocating %zd bytes\n", sizeof(config_t));
            return(NULL);
    }

    /* Define default options here */

    return newconfig;
}

int config_free(config_t *oldconfig) {

    if (oldconfig == NULL) {
        return -1;
    }

    /* free malloc'd strings within config */

    oldconfig = NULL;

    return 0;
}


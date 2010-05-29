/*
 *  conf.h
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

#ifndef _COWER_CONF_H
#define _COWER_CONF_H

struct config_t {
  /* operations */
  int op;

  /* options */
  unsigned short color;
  unsigned short getdeps;
  unsigned short force;
  unsigned short verbose;
  unsigned short quiet;
  unsigned short moreinfo;
  const char *download_dir;

  struct color_cfg_t {
    unsigned short repo;
    unsigned short pkg;
    unsigned short uptodate;
    unsigned short outofdate;
    unsigned short url;
    unsigned short info;
    unsigned short warn;
    unsigned short error;
  } *colors;
};

enum {
  OP_SEARCH = 1,
  OP_INFO = 2,
  OP_DL = 4,
  OP_UPDATE = 8
};

int config_free(struct config_t *oldconfig);
struct config_t *config_new(void);

/* global config variable */
extern struct config_t *config;

#endif /* _COWER_CONF_H */

/* vim: set ts=2 sw=2 et: */

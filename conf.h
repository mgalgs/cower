/* Copyright (c) 2010 Dave Reisner
 *
 * conf.h
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _COWER_CONF_H
#define _COWER_CONF_H

#include <alpm_list.h>

typedef enum __loglevel_t {
  LOG_INFO    = 1,
  LOG_ERROR   = (1 << 1),
  LOG_WARN    = (1 << 2),
  LOG_VERBOSE = (1 << 3),
  LOG_DEBUG   = (1 << 4)
} loglevel_t;

struct config_t {
  /* operations */
  int op;

  /* options */
  unsigned short color;
  unsigned short getdeps;
  unsigned short force;
  unsigned short quiet;
  unsigned short moreinfo;
  loglevel_t logmask;
  const char *download_dir;

  alpm_list_t *ignorepkgs;

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

  struct strings_t {
    char *repo;
    char *pkg;
    char *uptodate;
    char *outofdate;
    char *url;
    char *info;
    char *warn;
    char *error;
    char *c_off;
  } *strings;
};

enum {
  OP_SEARCH = 1,
  OP_INFO = 2,
  OP_DL = 4,
  OP_UPDATE = 8
};

enum {
  OP_IGNORE = 1000,
  OP_DEBUG = 1001,
};

int config_free(struct config_t *oldconfig);
struct config_t *config_new(void);

/* global config variable */
extern struct config_t *config;

#endif /* _COWER_CONF_H */

/* vim: set ts=2 sw=2 et: */

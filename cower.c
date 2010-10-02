/* Copyright (c) 2010 Dave Reisner
 *
 * cower.c
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

#define _GNU_SOURCE

#include <ctype.h> /* isspace */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* isatty */

#include "aur.h"
#include "conf.h"
#include "curl.h"
#include "download.h"
#include "pacman.h"
#include "query.h"
#include "update.h"
#include "util.h"

static alpm_list_t *targets; /* Package argument list */

static struct color_t {
  const char *name;
  unsigned short val;
} availcolors[] = {
  { "BG",                   BG }, { "BLACK",             BLACK },
  { "BLUE",               BLUE }, { "BOLDBG",           BOLDBG },
  { "BOLDBLACK",     BOLDBLACK }, { "BOLDBLUE",       BOLDBLUE },
  { "BOLDCYAN",       BOLDCYAN }, { "BOLDFG",           BOLDFG },
  { "BOLDGREEN",     BOLDGREEN }, { "BOLDMAGENTA", BOLDMAGENTA },
  { "BOLDRED",         BOLDRED }, { "BOLDWHITE",     BOLDWHITE },
  { "BOLDYELLOW",   BOLDYELLOW }, { "CYAN",               CYAN },
  { "FG",                   FG }, { "GREEN",             GREEN },
  { "MAGENTA",         MAGENTA }, { "RED",                 RED },
  { "WHITE",             WHITE }, { "YELLOW",           YELLOW }
};

static int fn_cmp_color(const void *c1, const void *c2) {
  struct color_t *color1 = (struct color_t*)c1;
  struct color_t *color2 = (struct color_t*)c2;

  return strcmp(color1->name, color2->name);
}

static unsigned short color_is_valid(const char *colorname) {
  struct color_t key, *result;
  key.name = colorname;

  result = bsearch(&key, availcolors, COLOR_MAX, sizeof(struct color_t), fn_cmp_color);

  if (result != NULL)
    return ((struct color_t*)result)->val;
  else
    return 0;
}

static void cleanup(int ret) {
  curl_easy_cleanup(curl);
  curl_global_cleanup();

  FREELIST(targets);
  alpm_release();
  config_free(config);

  exit(ret);
}

static int parseargs(int argc, char **argv) {
  int opt;
  int option_index = 0;
  static struct option opts[] = {
    /* Operations */
    {"search",    no_argument,        0, 's'},
    {"update",    no_argument,        0, 'u'},
    {"info",      no_argument,        0, 'i'},
    {"download",  no_argument,        0, 'd'},

    /* Options */
    {"color",     optional_argument,  0, 'c'},
    {"ignore",    required_argument,  0, OP_IGNORE},
    {"verbose",   no_argument,        0, 'v'},
    {"force",     no_argument,        0, 'f'},
    {"quiet",     no_argument,        0, 'q'},
    {"target",    required_argument,  0, 't'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "disucfqt:v", opts, &option_index))) {
    alpm_list_t *item = NULL, *list = NULL;
    if (opt < 0)
      break;

    switch (opt) {
      /* Operations */
      case 'd':
        if (! (config->op & OP_DL))
          config->op |= OP_DL;
        else
          config->getdeps = 1;
        break;
      case 'i':
        if (! (config->op & OP_INFO))
          config->op |= OP_INFO;
        else
          config->moreinfo = 1;
        break;
      case 's':
        config->op |= OP_SEARCH;
        break;
      case 'u':
        config->op |= OP_UPDATE;
        break;

      /* Options */
      case 'c':
        if (!optarg || STREQ(optarg, "auto")) {
          if (isatty(STDOUT_FILENO))
            config->color = 1;
          else
            config->color = 0;
        } else if (STREQ(optarg, "always"))
          config->color = 1;
        break;
      case 'f':
        config->force = 1;
        break;
      case 'q':
        config->quiet = 1;
        break;
      case 't':
        if (config->download_dir)
          FREE(config->download_dir);
        config->download_dir = relative_to_absolute_path(optarg);
        break;
      case 'v':
        config->verbose++;
        break;
      case OP_IGNORE:
        list = strsplit(optarg, ',');
        for (item = list; item; item = item->next) {
          if (alpm_list_find_str(config->ignorepkgs, item->data) == NULL) {
            config->ignorepkgs = alpm_list_add(config->ignorepkgs, strdup(item->data));
          }
        }
        break;

      case '?':
        return 1;
      default:
        return 1;
    }
  }

  /* Feed the remaining args into a linked list */
  while (optind < argc) {
    if (alpm_list_find_str(targets, argv[optind]) == NULL)
      targets = alpm_list_add(targets, strdup(argv[optind]));
    optind++;
  }

  return 0;
}

static int read_config_file() {
  int ret = 0;
  char *ptr, *xdg_config_home;
  char *config_path, line[BUFSIZ + 1];

  xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (xdg_config_home) {
    asprintf(&config_path, "%s/cower/cower.conf", xdg_config_home);
  } else { /* try to guess at where .config is */
    asprintf(&config_path, "%s/.config/cower/cower.conf", getenv("HOME"));
  }

  if (!file_exists(config_path)) {
    /* Return without error. Nothing bad happened, we
     * just don't have a config file to look at.
     */
    free(config_path);
    return ret;
  }

  FILE *conf_fd = fopen(config_path, "r");
  while (fgets(line, BUFSIZ, conf_fd)) {
    strtrim(line);

    if (line[0] == '#' || strlen(line) == 0)
      continue;

    if ((ptr = strchr(line, '#')))
      *ptr = '\0';

    char *key;
    unsigned short color;
    key = ptr = line;
    strsep(&ptr, "=");
    strtrim(key);
    strtrim(ptr);

    unsigned short *popt = NULL; /* shut up gcc */
    if (STREQ(key, "repo"))
      popt = &(config->colors->repo);
    else if (STREQ(key, "packages"))
      popt = &(config->colors->pkg);
    else if (STREQ(key, "uptodate"))
      popt = &(config->colors->uptodate);
    else if (STREQ(key, "outofdate"))
      popt = &(config->colors->outofdate);
    else if (STREQ(key, "url"))
      popt = &(config->colors->url);
    else if (STREQ(key, "info"))
      popt = &(config->colors->info);
    else if (STREQ(key, "warn"))
      popt = &(config->colors->warn);
    else if (STREQ(key, "error"))
      popt = &(config->colors->error);
    else {
      cfprintf(stderr, "%<::%> bad option found in config: '%s'\n", key);
      ret = 1;
      break;
    }

    if ((color = color_is_valid(ptr)) > 0)
      *popt = color;
    else {
      cfprintf(stderr, "%<::%> ignoring bad color found in config: '%s'\n", 
        config->colors->warn, ptr);
    }
  }

  fclose(conf_fd);
  free(config_path);

  return ret;
}

static void usage() {
printf("cower %s\n\
Usage: cower [options] <operation> PACKAGE [PACKAGE2..]\n\
\n\
 Operations:\n\
  -d, --download          Download PACKAGE(s). Pass twice to\n\
                            download AUR dependencies.\n\
  -i, --info              Show info for PACKAGE(s). Pass twice for\n\
                            more detail.\n\
  -s, --search            Search for PACKAGE(s).\n\
  -u, --update            Check for updates against AUR. If the \n\
                            --download flag is passed as well,\n\
                            fetch each available update.\n\n", VERSION);
printf(" General options:\n\
  -c, --color[=WHEN]      Use colored output. WHEN is `always' or `auto'.\n\
  -f, --force             Overwrite existing files when downloading.\n\
      --ignore <pkg>      Ignore a package upgrade (can be used more than once)\n\
  -q, --quiet             Output less.\n\
  -t, --target <dir>      Specify an alternate download directory.\n\
  -v, --verbose           Be more verbose.\n\n");
}

int main(int argc, char **argv) {
  if (argc == 1) {
    usage();
    cleanup(1);
  }

  int ret;

  config = config_new();

  ret = parseargs(argc, argv);
  if (ret > 0) {
    usage();
    cleanup(ret);
  }

  if (!config->op) {
    usage();
    cleanup(1);
  }

  /* read package arguments from stdin if we have none yet */
  if(!targets && !isatty(0)) {
    char line[BUFSIZ];
    int i = 0;
    while((line[i] = fgetc(stdin)) != EOF) {
      if(isspace(line[i])) {
        line[i] = 0;
        /* avoid adding zero length arg, if multiple spaces separate args */
        if(i > 0) {
          targets = alpm_list_add(targets, strdup(line));
          i = 0;
        }
      } else {
        ++i;
      }
    }
    /* end of stream -- check for data still in line buffer */
    if(i > 0) {
      targets = alpm_list_add(targets, strdup(line));
    }
    freopen(ctermid(NULL), "r", stdin);
  }

  if (config->color) {
    ret = read_config_file();
    if (ret > 0)
      cleanup(ret);
  }

  curl_global_init(CURL_GLOBAL_NOTHING);
  if (curl_local_init() != 0) {
    fprintf(stderr, "!! curl initialization failed. Please check your configuration.\n");
    cleanup(1);
  }

  /* Order matters somewhat. Update must come before download
   * to ensure that we catch the possibility of a download flag
   * being passed along with it.
   */
  if (config->op & OP_UPDATE) {
    ret = cower_do_update();
  } else if (config->op & OP_DL) {
    ret = cower_do_download(targets);
  } else if (config->op & OP_INFO) {
    alpm_list_t *results = cower_do_query(targets, AUR_QUERY_TYPE_INFO);
    if (results) {
      alpm_list_t *i;
      for (i = results; i; i = i->next)
        print_pkg_info(i->data);

      alpm_list_free_inner(results, aur_pkg_free);
      alpm_list_free(results);
    } else {
      ret = 2;
    }
  } else if (config->op & OP_SEARCH) {
    alpm_list_t *results = cower_do_query(targets, AUR_QUERY_TYPE_SEARCH);
    if (results) {
      print_pkg_search(results);
      alpm_list_free_inner(results, aur_pkg_free);
      alpm_list_free(results);
    } else {
      ret = 2;
    }
  }

  cleanup(ret);
  /* Never reached */
  return 0;
}

/* vim: set ts=2 sw=2 et: */

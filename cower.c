/*
 *  cower.c
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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aur.h"
#include "conf.h"
#include "curl.h"
#include "download.h"
#include "pacman.h"
#include "search.h"
#include "update.h"
#include "util.h"

static alpm_list_t *targets; /* Package argument list */

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
    {"color",     no_argument,        0, 'c'},
    {"verbose",   no_argument,        0, 'v'},
    {"force",     no_argument,        0, 'f'},
    {"quiet",     no_argument,        0, 'q'},
    {"target",    required_argument,  0, 't'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "disucfqt:v", opts, &option_index))) {
    if (opt < 0) {
      break;
    }

    switch (opt) {
      /* Operations */
      case 'd':
        if (! (config->op & OP_DL))
          config->op |= OP_DL;
        else
          config->getdeps = 1;
        break;
      case 'i':
        config->op |= OP_INFO;
        break;
      case 's':
        config->op |= OP_SEARCH;
        break;
      case 'u':
        config->op |= OP_UPDATE;
        break;

      /* Options */
      case 'c':
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
        config->download_dir = strndup(optarg, PATH_MAX);
        break;
      case 'v':
        config->verbose++;
        break;
      case '?': 
        return 1;
      default: 
        return 1;
    }
  }

  /* Feed the remaining args into a linked list */
  while (optind < argc)
    targets = alpm_list_add(targets, strdup(argv[optind++]));

  return 0;
}

static void usage() {
printf("cower %s\n\
Usage: cower [options] <operation> PACKAGE [PACKAGE2..]\n\
\n\
 Operations:\n\
  -d, --download          download PACKAGE(s)\n\
                            pass -dd to download AUR dependencies\n\
  -i, --info              show info for PACKAGE(s)\n\
  -s, --search            search for PACKAGE(s)\n\
  -u, --update            check for updates against AUR. If the \n\
                            --download flag is passed as well,\n\
                            fetch each available update.\n\n", VERSION);
printf(" General options:\n\
  -c, --color             use colored output\n\
  -f, --force             overwrite existing files when downloading\n\
  -q, --quiet             output less to stdout\n\
  -t DIR, --target=DIR    specify an alternate download directory\n\
  -v, --verbose           be more verbose\n\n");
}

int main(int argc, char **argv) {
  if (argc == 1) {
    usage();
    cleanup(1);
  }

  int ret;

  config = config_new();

  ret = parseargs(argc, argv);
  if (ret > 0)
    cleanup(ret);

  if (!config->op) {
    usage();
    cleanup(1);
  }

  curl_global_init(CURL_GLOBAL_NOTHING);
  curl_local_init();

  if (config->verbose >= 2) {
    printf("DEBUG => Options selected:\n");
    printf("\tconfig->op = %d\n", config->op);
    if (config->color) printf("\t--color\n");
    if (config->getdeps) printf("\t-dd\n");
    if (config->force) printf("\t--force\n");
    if (config->verbose > 0) printf("\t--verbose=%d\n", config->verbose);
    if (config->quiet) printf("\t--quiet\n");
    if (config->download_dir) printf("\t--target=%s\n", config->download_dir);
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
    }
  } else if (config->op & OP_SEARCH) {
    alpm_list_t *results = cower_do_query(targets, AUR_QUERY_TYPE_SEARCH);
    if (results) {
      print_pkg_search(results);
      alpm_list_free_inner(results, aur_pkg_free);
      alpm_list_free(results);
    }
  }

  cleanup(ret);
  /* Never reached */
  return 0;
}

/* vim: set ts=2 sw=2 et: */

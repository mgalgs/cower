/*
 *  util.c
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

/* Standard */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Non-standard */
#include <alpm.h>
#include <jansson.h>

/* Local */
#include "aur.h"
#include "conf.h"
#include "package.h"
#include "util.h"

static char *aur_cat[] = { NULL, "None", "daemons", "devel", "editors",
                           "emulators", "games", "gnome", "i18n", "kde", "lib",
                           "modules", "multimedia", "network", "office",
                           "science", "system", "x11", "xfce", "kernels" };


alpm_list_t *agg_search_results(alpm_list_t *agg, json_t *search) {

  alpm_list_t *i;
  aur_pkg_t *aur_pkg;
  int n;
  json_t *j_pkg;

  for (n = 0; n < json_array_size(search); n++) {
    j_pkg = json_array_get(search, n);
    aur_pkg = json_to_aur_pkg(j_pkg);

    agg = alpm_list_add_sorted(agg, aur_pkg, (alpm_list_fn_cmp)_aur_pkg_cmp);
  }

  return agg;

}

char *itoa(unsigned int num, int base){

   static char retbuf[33];
   char *p;

   if (base < 2 || base > 16)
     return NULL;

   p = &retbuf[sizeof(retbuf)-1];
   *p = '\0';

   do {
     *--p = "0123456789abcdef"[num % base];
     num /= base;
   } while (num != 0);

   return p;
}

/* Colorized printing with flexible output
 *
 * Limitation: only fixed width fields
 * Supports %d, %c, %s, %l
 * Pass a number to %! in fmt to turn on color
 * Use %@ to turn off color
 *
 * Returns: numbers of chars written (including term escape sequences)
 */
static int c_vfprintf(FILE *fd, const char* fmt, va_list args) {

  const char *p;
  int count = 0;

  int i; long l; char *s;

  for (p = fmt; *p != '\0'; p++) {
    if (*p != '%') {
      fputc(*p, fd); count++;
      continue;
    }

    switch (*++p) {
    case 'c':
      i = va_arg(args, int);
      fputc(i, fd); count++;
      break;
    case 's':
      s = va_arg(args, char*);
      count += fputs(s, fd);
      break;
    case 'd':
      i = va_arg(args, int);
      if (i < 0) {
        i = -i;
        fputc('-', fd); count++;
      }
      count += fputs(itoa(i, 10), fd);
      break;
    case 'l':
      l = va_arg(args, long);
      if (l < 0) {
        l = -l;
        fputc('-', fd); count++;
      }
      count += fputs(itoa(l, 10), fd);
      break;
    case '<': /* color on */
      count += fputs(C_ON, fd);
      count += fputs(itoa(va_arg(args, int), 10), fd);
      fputc('m', fd); count++;
      break;
    case '>': /* color off */
      count += fputs(C_OFF, fd);
      break;
    case '%':
      fputc('%', fd); count++;
      break;
    }
  }

  return count;
}

int cfprintf(FILE *fd, const char* fmt, ...) {
  va_list args;
  int result;

  va_start(args, fmt);
  result = c_vfprintf(fd, fmt, args);
  va_end(args);

  return result;
}

int cprintf(const char* fmt, ...) {
  va_list args;
  int result;

  va_start(args, fmt);
  result = c_vfprintf(stdout, fmt, args);
  va_end(args);

  return result;
}

void print_pkg_info(json_t *pkg) {

  json_t *pkginfo;
  const char *id, *name, *ver, *url, *cat, *license, *votes, *desc;
  int ood;

  pkginfo = json_object_get(pkg, "results");

  /* Declare  to json data to make my life easier */
  id      = json_string_value(json_object_get(pkginfo, "ID"));
  name    = json_string_value(json_object_get(pkginfo, "Name"));
  ver     = json_string_value(json_object_get(pkginfo, "Version"));
  url     = json_string_value(json_object_get(pkginfo, "URL"));
  cat     = json_string_value(json_object_get(pkginfo, "CategoryID"));
  license = json_string_value(json_object_get(pkginfo, "License"));
  votes   = json_string_value(json_object_get(pkginfo, "NumVotes"));
  desc    = json_string_value(json_object_get(pkginfo, "Description"));
  ood     = atoi(json_string_value(json_object_get(pkginfo, "OutOfDate")));

  if (config->color) {
    cprintf("Repository      : %<aur%>\n", MAGENTA);
    cprintf("Name            : %<%s%>\n", WHITE, name);
    cprintf("Version         : %<%s%>\n", ood ? RED : GREEN, ver);
    cprintf("URL             : %<%s%>\n", CYAN, url);
    cprintf("AUR Page        : %<%s%s%>\n", CYAN, AUR_PKG_URL_FORMAT, id);
  } else {
    printf("Repository      : aur\n");
    printf("Name:           : %s\n", name);
    printf("Version         : %s\n", ver);
    printf("URL             : %s\n", url);
    printf("AUR Page        : %s%s\n", AUR_PKG_URL_FORMAT, id);
  }

  printf("Category        : %s\n", aur_cat[atoi(cat)]);
  printf("License         : %s\n", license);
  printf("Number of Votes : %s\n", votes);

  if (config->color) {
    cprintf("Out of Date     : %<%s%>\n", ood ? RED : GREEN, ood ? "Yes" : "No");
  } else {
    printf("Out of Date     : %s\n", ood ? "Yes" : "No");
  }

  printf("Description     : %s\n\n", desc);

}

void print_pkg_search(json_t *search) {

  json_t *pkg_array, *pkg;
  unsigned int i;
  const char *name, *ver, *desc;
  int ood;

  pkg_array = json_object_get(search, "results");

  for (i = 0; i < json_array_size(pkg_array); i++) {
    pkg = json_array_get(pkg_array, i);

    name = json_string_value(json_object_get(pkg, "Name"));
    ver  = json_string_value(json_object_get(pkg, "Version"));
    desc = json_string_value(json_object_get(pkg, "Description"));
    ood  = atoi(json_string_value(json_object_get(pkg, "OutOfDate")));

    if (config->quiet) {
      if (config->color) {
        cprintf("%<%s%>\n", WHITE, name);
      } else {
        printf("%s\n", name);
      }
    } else {
      if (config->color) {
        cprintf("%<aur/%>%<%s%> %<%s%>\n",
          MAGENTA, WHITE, name, ood ? RED : GREEN, ver);
      } else {
        printf("aur/%s %s\n", name, ver);
      }
      printf("    %s\n", desc);
    }

  }
}

int file_exists(const char* filename) {

  struct stat st;

  return ! stat(filename, &st);
}


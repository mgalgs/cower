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

/* standard */
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* local */
#include "aur.h"
#include "conf.h"
#include "download.h"
#include "util.h"

static char *aur_cat[] = { NULL, "None", "daemons", "devel", "editors",
                           "emulators", "games", "gnome", "i18n", "kde", "lib",
                           "modules", "multimedia", "network", "office",
                           "science", "system", "x11", "xfce", "kernels" };


/** 
* @brief slimmed down printf with added patterns for color
* 
* @param fd       file descriptor to write to
* @param fmt      string describing output
* @param args     data to replace patterns in fmt
* 
* @return number of characters printed
*/
static int c_vfprintf(FILE *fd, const char* fmt, va_list args) {
  const char *p;
  int color, count = 0;
  char cprefix[10] = {0};

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
      color = va_arg(args, int);
      snprintf(cprefix, 10, C_ON, color / 10, color % 10);
      count += fputs(cprefix, fd);
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

/**
* @brief aggregate a search JSON array into an alpm_list_t
*
* @param agg      alpm_list_t to aggregate into
* @param search   JSON to be aggregated
*
* @return head of the alpm_list_t
*/
alpm_list_t *agg_search_results(alpm_list_t *haystack, alpm_list_t *addthis) {
  haystack = alpm_list_mmerge_dedupe(haystack, addthis, (alpm_list_fn_cmp)aur_pkg_cmp,
    (alpm_list_fn_free)aur_pkg_free);

  return haystack;
}


/** 
* @brief front end to c_vfprintf
* 
* @param fd       file descriptor to write to
* @param fmt      format describing output
* @param ...      data to replace patterns in fmt
* 
* @return number of characters written
*/
int cfprintf(FILE *fd, const char *fmt, ...) {
  va_list args;
  int result;

  va_start(args, fmt);
  result = c_vfprintf(fd, fmt, args);
  va_end(args);

  return result;
}

/** 
* @brief front end to c_vfprintf
* 
* @param fmt      format describing output
* @param ...      data to replace patterns in fmt
* 
* @return number of characters written
*/
int cprintf(const char *fmt, ...) {
  va_list args;
  int result;

  va_start(args, fmt);
  result = c_vfprintf(stdout, fmt, args);
  va_end(args);

  return result;
}

/** 
* @brief check for existance of a file or directory
* 
* @param filename file or directory to check for existance of
* 
* @return 1 if exists, else 0
*/
int file_exists(const char *filename) {
  struct stat st;

  return stat(filename, &st) == 0;
}

off_t filesize(const char *filename) {
  struct stat st;

  stat(filename, &st);

  return st.st_size;
}

/** 
* @brief convert int to ascii representation
* 
* @param num      number to convert
* @param base     numerical base to convert to
* 
* @return ascii representation of the num parameter
*/
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

/** 
* @brief print full information about a AUR package
* 
* @param pkg      JSON describing the package
*/
void print_pkg_info(struct aur_pkg_t *pkg) {

  if (config->color) {
    cprintf("Repository      : %<aur%>\n"
            "Name            : %<%s%>\n"
            "Version         : %<%s%>\n"
            "URL             : %<%s%>\n"
            "AUR Page        : %<%s%s%>\n",
            config->colors->repo,
            config->colors->pkg, pkg->name,
            pkg->ood ? config->colors->outofdate : config->colors->uptodate, pkg->ver,
            config->colors->url, pkg->url,
            config->colors->url, AUR_PKG_URL_FORMAT, pkg->id);
  } else {
    printf("Repository      : aur\n"
           "Name:           : %s\n"
           "Version         : %s\n"
           "URL             : %s\n"
           "AUR Page        : %s%s\n",
           pkg->name,
           pkg->ver,
           pkg->url,
           AUR_PKG_URL_FORMAT, pkg->id);

  }

  printf("Category        : %s\n"
         "License         : %s\n"
         "Number of Votes : %s\n",
         aur_cat[atoi(pkg->cat)],
         pkg->lic,
         pkg->votes);

  if (config->color) {
    cprintf("Out of Date     : %<%s%>\n", pkg->ood ? 
      config->colors->outofdate : config->colors->uptodate, pkg->ood ? "Yes" : "No");
  } else {
    printf("Out of Date     : %s\n", pkg->ood ? "Yes" : "No");
  }

  printf("Description     : %s\n\n", pkg->desc);
}

/** 
* @brief print aggregated search results.
* 
* @param search     alpm_list_t packed with aur_pkg_t structs.
*/
void print_pkg_search(alpm_list_t *search) {
  alpm_list_t *i;
  struct aur_pkg_t *pkg;

  for (i = search; i; i = i->next) {
    pkg = i->data;

    if (config->quiet) {
      if (config->color)
        cprintf("%<%s%>\n", config->colors->pkg, pkg->name);
      else
        printf("%s\n", pkg->name);
    } else {
      if (config->color)
        cprintf("%<aur/%>%<%s%> %<%s%>\n",
          config->colors->repo, config->colors->pkg, pkg->name, pkg->ood ?
            config->colors->outofdate : config->colors->uptodate, pkg->ver);
      else
        printf("aur/%s %s\n", pkg->name, pkg->ver);
      printf("    %s\n", pkg->desc);
    }
  }
}

/** 
* @brief print line indicating a package update
* 
* @param pkg          package name
* @param local_ver    locally installed version
* @param remote_ver   remotely available version
*/
void print_pkg_update(const char *pkg, const char *local_ver, const char *remote_ver) {
  if (config->color) {
    cprintf("%<%s%>\n", config->colors->pkg, pkg);
    if (! config->quiet)
      cprintf(" %<%s%> -> %<%s%>\n", config->colors->outofdate, local_ver,
        config->colors->uptodate, remote_ver);
  } else {
    if (! config->quiet)
      printf("%s %s -> %s\n", pkg, local_ver, remote_ver);
    else
      printf("%s\n", pkg);
  }
}

/** 
* @brief trim whitespace from string
* 
* @param str    string to trim
* 
* @return trimmed string
*/
char *strtrim(char *str) {

  char *pch = str;

  if(str == NULL || *str == '\0')
    return(str);

  while(isspace(*pch))
    pch++;

  if(pch != str)
    memmove(str, pch, (strlen(pch) + 1));

  if(*str == '\0')
    return(str);

  pch = (str + strlen(str) - 1);

  while(isspace(*pch))
    pch--;

  *++pch = '\0';

  return(str);
}

/* vim: set ts=2 sw=2 et: */

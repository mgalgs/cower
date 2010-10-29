/* Copyright (c) 2010 Dave Reisner
 *
 * util.c
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

/* standard */
#define _GNU_SOURCE
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

/* local */
#include "aur.h"
#include "conf.h"
#include "download.h"
#include "util.h"

static char *aur_cat[] = { NULL, "None", "daemons", "devel", "editors",
                           "emulators", "games", "gnome", "i18n", "kde", "lib",
                           "modules", "multimedia", "network", "office",
                           "science", "system", "x11", "xfce", "kernels" };


int cwr_vfprintf(FILE *stream, loglevel_t level, const char *format, va_list args) {
  int ret = 0;

  if (!(config->logmask & level)) {
    return ret;
  }

  switch(level) {
    case LOG_ERROR:
      fprintf(stream, "%s%s ", config->strings->error, config->strings->c_off);
      break;
    case LOG_WARN:
      fprintf(stream, "%s%s ", config->strings->warn, config->strings->c_off);
      break;
    case LOG_INFO:
      fprintf(stream, "%s%s ", config->strings->info, config->strings->c_off);
      break;
    case LOG_DEBUG:
      fprintf(stream, "debug: ");
      break;
    default:
      break;
  }

  ret = vfprintf(stream, format, args);
  return(ret);
}

int cwr_asprintf(char **string, const char *format, ...) {
  int ret = 0;
  va_list args;

  va_start(args, format);
  if (vasprintf(string, format, args) == -1) {
    cwr_fprintf(stderr, LOG_ERROR, "failed to allocate string\n");
    ret = -1;
  }
  va_end(args);

  return(ret);
}

int cwr_fprintf(FILE *stream, loglevel_t level, const char *format, ...) {
  int ret;
  va_list args;

  va_start(args, format);
  ret = cwr_vfprintf(stream, level, format, args);
  va_end(args);

  return(ret);
}

int cwr_printf(loglevel_t level, const char *format, ...) {
  int ret;
  va_list args;

  va_start(args, format);
  ret = cwr_vfprintf(stdout, level, format, args);
  va_end(args);

  return(ret);
}

int file_exists(const char *filename) {
  struct stat st;

  return stat(filename, &st) == 0;
}

off_t filesize(const char *filename) {
  struct stat st;

  stat(filename, &st);

  return st.st_size;
}

static int get_screen_width(void) {
  if(!isatty(1))
    return 80;

  struct winsize win;
  if(ioctl(1, TIOCGWINSZ, &win) == 0)
    return win.ws_col;

  return 80;
}

char *get_file_as_buffer(const char *filename) {
  FILE *fd;
  char *buf;
  off_t fsize;
  size_t nread;

  fsize = filesize(filename);

  if (!fsize)
    return NULL;

  buf = calloc(1, fsize + 1);

  fd = fopen(filename, "r");
  nread = fread(buf, 1, fsize, fd);
  fclose(fd);

  return nread == 0 ? NULL : buf;
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

void print_pkg_info(struct aur_pkg_t *pkg) {
  size_t max_line_len = get_screen_width() - INDENT - 1;

  printf("Repository      : %saur%s\n"
         "Name            : %s%s%s\n"
         "Version         : %s%s%s\n"
         "URL             : %s%s%s\n"
         "AUR Page        : %s%s%d%s\n",
         config->strings->repo, config->strings->c_off,
         config->strings->pkg, pkg->name, config->strings->c_off,
         pkg->ood ? config->strings->outofdate : config->strings->uptodate,
         pkg->ver, config->strings->c_off,
         config->strings->url, pkg->url, config->strings->c_off,
         config->strings->url, AUR_PKG_URL_FORMAT, pkg->id, config->strings->c_off);

  if (config->moreinfo) {
    print_extinfo_list(PKG_OUT_PROVIDES, pkg->provides, max_line_len, INDENT);
    print_extinfo_list(PKG_OUT_DEPENDS, pkg->depends, max_line_len, INDENT);
    print_extinfo_list(PKG_OUT_MAKEDEPENDS, pkg->makedepends, max_line_len, INDENT);

    /* Always making excuses for optdepends... */
    if (pkg->optdepends) {
      printf("Optdepends      : ");

      printf("%s\n", (const char*)pkg->optdepends->data);

      alpm_list_t *i;
      for (i = pkg->optdepends->next; i; i = i->next) {
        printf("%*s%s\n", INDENT, "", (const char*)i->data);
      }
    }

    print_extinfo_list(PKG_OUT_CONFLICTS, pkg->conflicts, max_line_len, INDENT);
    print_extinfo_list(PKG_OUT_REPLACES, pkg->replaces, max_line_len, INDENT);
  }

  printf("%-*s: %s\n%-*s: %s\n%-*s: %d\n",
      INDENT - 2, PKG_OUT_CAT, aur_cat[pkg->cat],
      INDENT - 2, PKG_OUT_LICENSE, pkg->lic,
      INDENT - 2, PKG_OUT_NUMVOTES, pkg->votes);

  printf("Out of Date     : %s%s%s\n", pkg->ood ? 
      config->strings->outofdate : config->strings->uptodate, pkg->ood ? "Yes" : "No",
      config->strings->c_off);

  printf("%-*s: ", INDENT - 2, PKG_OUT_DESC);

  size_t desc_len = strlen(pkg->desc);
  if (desc_len < max_line_len) {
    printf("%s\n", pkg->desc);
  } else {
    print_wrapped(pkg->desc, max_line_len, INDENT);
  }

  putchar('\n');
}

void print_pkg_search(alpm_list_t *search) {
  alpm_list_t *i;
  struct aur_pkg_t *pkg;

  for (i = search; i; i = i->next) {
    pkg = i->data;

    if (config->quiet) {
      printf("%s%s%s\n", config->strings->pkg, pkg->name, config->strings->c_off);
    } else {
      printf("%saur/%s%s %s%s%s\n    %s\n",
          config->strings->repo, config->strings->pkg, pkg->name,
          pkg->ood ? config->strings->outofdate : config->strings->uptodate,
          pkg->ver, config->strings->c_off, pkg->desc);
    }

  }
}

void print_pkg_update(const char *pkg, const char *local_ver, const char *remote_ver) {
  if (config->quiet) {
    printf("%s%s%s\n", config->strings->pkg, pkg, config->strings->c_off);
  } else {
    cwr_printf(LOG_INFO, "%s%s %s%s%s -> %s%s%s\n",
        config->strings->pkg, pkg,
        config->strings->outofdate, local_ver, config->strings->c_off,
        config->strings->uptodate, remote_ver, config->strings->c_off);
  }
}

void print_extinfo_list(const char *field, alpm_list_t *list, size_t max_line_len, int indent) {
  if (!list) {
    return;
  }

  printf("%-*s: ", indent - 2, field);

  int count = 0;
  size_t deplen;
  alpm_list_t *i;
  for (i = list; i; i = i->next) {
    deplen = strlen(i->data);
    if (count + deplen >= max_line_len) {
      printf("%-*s", indent + 1, "\n");
      count = 0;
    }
    count += printf("%s  ", (const char*)i->data);
  }
  putchar('\n');
}

void print_wrapped(const char* buffer, size_t maxlength, int indent) {
  unsigned pos, lastSpace;

  pos = lastSpace = 0;
  while(buffer[pos] != 0) {
    int isLf = (buffer[pos] == '\n');

    if (isLf || pos == maxlength) {
      if (isLf || lastSpace == 0)
        lastSpace = pos;

      while(*buffer != 0 && lastSpace-- > 0)
        putchar(*buffer++);

      putchar('\n');
      if (indent)
        printf("%*s", indent, "");

      if (isLf) /* newline in the stream, skip it */
        buffer++;

      while (*buffer && isspace(*buffer))
        buffer++;

      lastSpace = pos = 0;
    } else {
      if (isspace(buffer[pos]))
        lastSpace = pos;

      pos++;
    }
  }
  printf("%s\n", buffer);
}

char *ltrim(char *str) {
  char *pch = str;

  if (str == NULL || *str == '\0')
    return str;

  while (isspace(*pch))
    pch++;

  if (pch != str)
    memmove(str, pch, (strlen(pch) + 1));

  return str;
}

char *strtrim(char *str) {
  char *pch = str;

  if (str == NULL || *str == '\0')
    return str;

  while (isspace(*pch))
    pch++;

  if (pch != str)
    memmove(str, pch, (strlen(pch) + 1));

  if (*str == '\0')
    return(str);

  pch = (str + strlen(str) - 1);

  while (isspace(*pch))
    pch--;

  *++pch = '\0';

  return str;
}

alpm_list_t *strsplit(const char *str, const char splitchar) {
  alpm_list_t *list = NULL;
  const char *prev = str;
  char *dup = NULL;

  while((str = strchr(str, splitchar))) {
    dup = strndup(prev, str - prev);
    if(dup == NULL) {
      return(NULL);
    }
    list = alpm_list_add(list, dup);

    str++;
    prev = str;
  }

  dup = strdup(prev);
  if(dup == NULL) {
    return(NULL);
  }
  list = alpm_list_add(list, dup);

  return(list);
}

char *relative_to_absolute_path(const char *relpath) {
  if (*relpath == '/') { /* already absolute */
    return strdup(relpath);
  }

  char *abspath = NULL;

  abspath = getcwd(abspath, PATH_MAX + 1);
  abspath = strncat(abspath, "/", PATH_MAX - strlen(abspath));
  abspath = strncat(abspath, relpath, PATH_MAX - strlen(abspath));

  return abspath;

}

/* vim: set ts=2 sw=2 et: */

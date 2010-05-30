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

int cfprintf(FILE *fd, const char *fmt, ...) {
  va_list args;
  int result;

  va_start(args, fmt);
  result = c_vfprintf(fd, fmt, args);
  va_end(args);

  return result;
}

int cprintf(const char *fmt, ...) {
  va_list args;
  int result;

  va_start(args, fmt);
  result = c_vfprintf(stdout, fmt, args);
  va_end(args);

  return result;
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

  fsize = filesize(filename);

  if (!fsize)
    return NULL;

  buf = calloc(1, fsize + 1);

  fd = fopen(filename, "r");
  fread(buf, 1, fsize, fd);
  fclose(fd);

  return buf;
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
  size_t max_line_len = get_screen_width() - 17;

  if (config->color) {
    cprintf("Repository      : %<aur%>\n"
            "Name            : %<%s%>\n"
            "Version         : %<%s%>\n"
            "URL             : %<%s%>\n"
            "AUR Page        : %<%s%d%>\n",
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
           "AUR Page        : %s%d\n",
           pkg->name,
           pkg->ver,
           pkg->url,
           AUR_PKG_URL_FORMAT, pkg->id);

  }

  if (pkg->provides) {
    printf("Provides        : ");
    size_t deplen, count = 0;
    alpm_list_t *i;
    for (i = pkg->provides; i; i = i->next) {
      deplen = strlen(i->data);
      if (count + deplen > max_line_len) {
        printf("\n                  ");
        count = 0;
      }

      count += printf("%s  ", (const char*)i->data);
    }

    putchar('\n');
  }

  if (pkg->depends) {
    printf("Depends         : ");

    int count = 0;
    size_t deplen;
    alpm_list_t *i;
    for (i = pkg->depends; i; i = i->next) {
      deplen = strlen(i->data);
      if (count + deplen > max_line_len) {
        printf("\n                  ");
        count = 0;
      }

      count += printf("%s  ", (const char*)i->data);
    }

    putchar('\n');
  }

  if (pkg->makedepends) {
    printf("Makedepends     : ");

    int count = 0;
    size_t deplen;
    alpm_list_t *i;
    for (i = pkg->makedepends; i; i = i->next) {
      deplen = strlen(i->data);
      if (count + deplen > max_line_len) {
        printf("\n                  ");
        count = 0;
      }

      count += printf("%s  ", (const char*)i->data);
    }

    putchar('\n');
  }

  if (pkg->optdepends) {
    printf("Optdepends      : ");

    printf("%s\n", (const char*)pkg->optdepends->data);

    alpm_list_t *i;
    for (i = pkg->optdepends->next; i; i = i->next) {
      printf("                  %s\n", (const char*)i->data);
    }
  }

  if (pkg->conflicts) {
    printf("Conflicts       : ");
    int count = 0;
    size_t deplen;
    alpm_list_t *i;
    for (i = pkg->conflicts; i; i = i->next) {
      deplen = strlen(i->data);
      if (count + deplen > max_line_len) {
        printf("\n                  ");
        count = 0;
      }

      count += printf("%s  ", (const char*)i->data);
    }

    putchar('\n');
  }

  if (pkg->replaces) {
    printf("Replaces        : ");
    int count = 0;
    size_t deplen;
    alpm_list_t *i;
    for (i = pkg->replaces; i; i = i->next) {
      deplen = strlen(i->data);
      if (count + deplen > max_line_len) {
        printf("\n                  ");
        count = 0;
      }

      count += printf("%s  ", (const char*)i->data);
    }

    putchar('\n');
  }

  printf("Category        : %s\n"
         "License         : %s\n"
         "Number of Votes : %d\n",
         aur_cat[pkg->cat],
         pkg->lic,
         pkg->votes);

  if (config->color) {
    cprintf("Out of Date     : %<%s%>\n", pkg->ood ? 
      config->colors->outofdate : config->colors->uptodate, pkg->ood ? "Yes" : "No");
  } else {
    printf("Out of Date     : %s\n", pkg->ood ? "Yes" : "No");
  }

  printf("Description     : ");

  size_t desc_len = strlen(pkg->desc);
  if (desc_len < max_line_len)
    printf("%s\n", pkg->desc);
  else
    print_wrapped(pkg->desc, max_line_len, 17);

  putchar('\n');

}

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

void print_pkg_update(const char *pkg, const char *local_ver, const char *remote_ver) {
  if (config->color) {
    if (! config->quiet)
      cprintf("%<%s%> %<%s%> -> %<%s%>\n", config->colors->pkg, pkg, 
        config->colors->outofdate, local_ver, config->colors->uptodate, remote_ver);
    else
      cprintf("%<%s%>\n", config->colors->pkg, pkg);
  } else {
    if (! config->quiet)
      printf("%s %s -> %s\n", pkg, local_ver, remote_ver);
    else
      printf("%s\n", pkg);
  }
}

void print_wrapped(const char* buffer, size_t maxlength, size_t indent) {
  size_t count, buflen;
  const char *ptr, *endptr;

  count = 0;
  buflen = strlen(buffer);

  do {
    ptr = buffer + count;

    /* don't set endptr beyond the end of the buffer */
    if (ptr - buffer + maxlength <= buflen)
      endptr = ptr + maxlength;
    else
      endptr = buffer + buflen;

    /* back up EOL to a null terminator or space */
    while (*(endptr) && !isspace(*(endptr)) )
      endptr--;

    count += fwrite(ptr, 1, endptr - ptr, stdout);

    /* print a newline and an indent */
    putchar('\n');
    size_t i;
    for (i = 0; i < indent; i++)
      putchar(' ');
  } while (*endptr);
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

/* vim: set ts=2 sw=2 et: */

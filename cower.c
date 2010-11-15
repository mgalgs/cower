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

/* glibc */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <wchar.h>

/* external libs */
#include <alpm.h>
#include <alpm_list.h>
#include <archive.h>
#include <archive_entry.h>
#include <curl/curl.h>
#include <yajl/yajl_parse.h>

/* macros */
#define FREE(x)               do { if(x) free((void*)x); x = NULL; } while(0)
#define STREQ(x,y)            (strcmp((x),(y)) == 0)
#define STR_STARTS_WITH(x,y)  (strncmp((x),(y), strlen(y)) == 0)

#define COWER_USERAGENT       "cower/3.x"

#define HTTP                  "http"
#define HTTPS                 "https"
#define AUR_PKGBUILD_PATH     "%s://aur.archlinux.org/packages/%s/%s/PKGBUILD"
#define AUR_PKG_URL           "%s://aur.archlinux.org/packages/%s/%s.tar.gz"
#define AUR_PKG_URL_FORMAT    "%s://aur.archlinux.org/packages.php?ID="
#define AUR_RPC_URL           "%s://aur.archlinux.org/rpc.php?type=%s&arg=%s"
#define AUR_MAX_CONNECTIONS   10

#define AUR_QUERY_TYPE        "type"
#define AUR_QUERY_TYPE_INFO   "info"
#define AUR_QUERY_TYPE_SEARCH "search"
#define AUR_QUERY_ERROR       "error"

#define NAME                  "Name"
#define VERSION               "Version"
#define URL                   "URL"

#define AUR_ID                "ID"
#define AUR_CAT               "CategoryID"
#define AUR_DESC              "Description"
#define AUR_URLPATH           "URLPath"
#define AUR_LICENSE           "License"
#define AUR_VOTES             "NumVotes"
#define AUR_OOD               "OutOfDate"

#define PKGBUILD_DEPENDS      "depends=("
#define PKGBUILD_MAKEDEPENDS  "makedepends=("
#define PKGBUILD_OPTDEPENDS   "optdepends=("
#define PKGBUILD_PROVIDES     "provides=("
#define PKGBUILD_CONFLICTS    "conflicts=("
#define PKGBUILD_REPLACES     "replaces=("

#define PKG_REPO              "Repository"
#define PKG_AURPAGE           "AUR Page"
#define PKG_PROVIDES          "Provides"
#define PKG_DEPENDS           "Depends On"
#define PKG_MAKEDEPENDS       "Makedepends"
#define PKG_OPTDEPENDS        "Optional Deps"
#define PKG_CONFLICTS         "Conflicts With"
#define PKG_REPLACES          "Replaces"
#define PKG_CAT               "Category"
#define PKG_NUMVOTES          "Votes"
#define PKG_LICENSE           "License"
#define PKG_OOD               "Out of Date"
#define PKG_DESC              "Description"

#define INFO_INDENT           17
#define SRCH_INDENT           4

#define NC                    "\033[0m"
#define BOLD                  "\033[1m"

#define BLACK                 "\033[0;30m"
#define RED                   "\033[0;31m"
#define GREEN                 "\033[0;32m"
#define YELLOW                "\033[0;33m"
#define BLUE                  "\033[0;34m"
#define MAGENTA               "\033[0;35m"
#define CYAN                  "\033[0;36m"
#define WHITE                 "\033[0;37m"
#define BOLDBLACK             "\033[1;30m"
#define BOLDRED               "\033[1;31m"
#define BOLDGREEN             "\033[1;32m"
#define BOLDYELLOW            "\033[1;33m"
#define BOLDBLUE              "\033[1;34m"
#define BOLDMAGENTA           "\033[1;35m"
#define BOLDCYAN              "\033[1;36m"
#define BOLDWHITE             "\033[1;37m"

/* enums */
typedef enum __loglevel_t {
  LOG_INFO    = 1,
  LOG_ERROR   = (1 << 1),
  LOG_WARN    = (1 << 2),
  LOG_DEBUG   = (1 << 3)
} loglevel_t;

typedef enum __operation_t {
  OP_SEARCH   = 1,
  OP_INFO     = (1 << 1),
  OP_DOWNLOAD = (1 << 2),
  OP_UPDATE   = (1 << 3)
} operation_t;

enum {
  OP_DEBUG = 1000,
  OP_SSL,
  OP_IGNORE
};

enum {
  PKGDETAIL_DEPENDS = 0,
  PKGDETAIL_MAKEDEPENDS,
  PKGDETAIL_OPTDEPENDS,
  PKGDETAIL_PROVIDES,
  PKGDETAIL_CONFLICTS,
  PKGDETAIL_REPLACES,
  PKGDETAIL_MAX
};

struct strings_t {
  const char *error;
  const char *warn;
  const char *info;
  const char *pkg;
  const char *repo;
  const char *url;
  const char *ood;
  const char *utd;
  const char *nc;
};

struct aurpkg_t {
  const char *id;
  const char *name;
  const char *ver;
  const char *desc;
  const char *url;
  const char *urlpath;
  const char *lic;
  const char *votes;
  int cat;
  int ood;

  alpm_list_t *depends;
  alpm_list_t *makedepends;
  alpm_list_t *optdepends;
  alpm_list_t *provides;
  alpm_list_t *conflicts;
  alpm_list_t *replaces;
};

struct yajl_parser_t {
  alpm_list_t *pkglist;
  struct aurpkg_t *aurpkg;
  char curkey[32];
  int json_depth;
};

struct response_t {
  char *data;
  size_t size;
};

/* function declarations */
static alpm_list_t *alpm_find_foreign_pkgs(void);
static int alpm_init(void);
static alpm_list_t *alpm_list_mmerge_dedupe(alpm_list_t*, alpm_list_t*, alpm_list_fn_cmp, alpm_list_fn_free);
static alpm_list_t *alpm_list_remove_item(alpm_list_t*, alpm_list_fn_free);
static int alpm_pkg_is_foreign(pmpkg_t*);
static pmdb_t *alpm_provides_pkg(const char*);
static int archive_extract_file(const struct response_t*);
static int aurpkg_cmp(const void*, const void*);
static void aurpkg_free(void*);
static struct aurpkg_t *aurpkg_new(void);
static CURL *curl_create_easy_handle(void);
static char *curl_get_url_as_buffer(const char*);
static size_t curl_write_response(void*, size_t, size_t, void*);
static int cwr_asprintf(char**, const char*, ...) __attribute__((format(printf,2,3)));
static int cwr_fprintf(FILE*, loglevel_t, const char*, ...) __attribute__((format(printf,3,4)));
static int cwr_printf(loglevel_t, const char*, ...) __attribute__((format(printf,2,3)));
static int cwr_vfprintf(FILE*, loglevel_t, const char*, va_list) __attribute__((format(printf,3,0)));
static int file_exists(const char*);
static char *get_file_as_buffer(const char*);
static int getcols(void);
static void indentprint(const char*, int);
static int json_end_map(void*);
static int json_map_key(void*, const unsigned char*, unsigned int);
static int json_start_map(void*);
static int json_string(void*, const unsigned char*, unsigned int);
static alpm_list_t *parse_bash_array(alpm_list_t*, char**, int);
static int parse_options(int, char*[]);
static void pkgbuild_get_extinfo(char**, alpm_list_t**[]);
static void print_extinfo_list(alpm_list_t*, const char*);
static void print_pkg_info(struct aurpkg_t*);
static void print_pkg_search(struct aurpkg_t*);
static void print_results(alpm_list_t*, void (*)(struct aurpkg_t*));
static int resolve_dependencies(const char*);
static int strings_init(void);
static char *strtrim(char*);
static void *thread_download(void*);
static void *thread_query(void*);
static void *thread_update(void*);
static void usage(void);
static int verify_download_path(void);
static size_t yajl_parse_stream(void*, size_t, size_t, void*);

/* options */
char *download_dir = NULL;
int optcolor = 0;
int optforce = 0;
int optquiet = 0;
char *optproto = HTTP;
int getdepends = 0;
int optextinfo = 0;

/* variables */
loglevel_t logmask = LOG_ERROR|LOG_WARN|LOG_INFO;
operation_t opmask = 0;
pmdb_t *db_local;
sem_t sem_download;
alpm_list_t *ignore = NULL, *targets = NULL;
struct strings_t *colstr;

static yajl_callbacks callbacks = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  json_string,
  json_start_map,
  json_map_key,
  json_end_map,
  NULL,
  NULL
};

static char *aur_cat[] = { NULL, "None", "daemons", "devel", "editors",
                           "emulators", "games", "gnome", "i18n", "kde", "lib",
                           "modules", "multimedia", "network", "office",
                           "science", "system", "x11", "xfce", "kernels" };

/* function implementations */
int alpm_init() {
  int ret = 0;
  FILE *fp;
  char line[PATH_MAX];
  char *ptr, *section = NULL;

  cwr_printf(LOG_DEBUG, "initializing alpm\n");

  ret = alpm_initialize();
  if (ret != 0) {
    return(ret);
  }

  ret = alpm_option_set_root("/");
  if (ret != 0) {
    return(ret);
  }
  cwr_printf(LOG_DEBUG, "setting alpm RootDir to %s\n", alpm_option_get_root());

  ret = alpm_option_set_dbpath("/var/lib/pacman");
  if (ret != 0) {
    return(ret);
  }
  cwr_printf(LOG_DEBUG, "setting alpm DBPath to: %s\n", alpm_option_get_dbpath());

  db_local = alpm_db_register_local();
  if (!db_local) {
    return(1);
  }

  fp = fopen("/etc/pacman.conf", "r");
  if (!fp) {
    return(1);
  }

  /* only light error checking happens here. this is pacman's job, not mine */
  while (fgets(line, PATH_MAX, fp)) {
    strtrim(line);

    if (strlen(line) == 0 || line[0] == '#') {
      continue;
    }
    if ((ptr = strchr(line, '#'))) {
      *ptr = '\0';
    }

    if (line[0] == '[' && line[strlen(line) - 1] == ']') {
      ptr = &line[1];
      if (section) {
        free(section);
      }

      section = strdup(ptr);
      section[strlen(section) - 1] = '\0';

      if (strcmp(section, "options") != 0) {
        if (!alpm_db_register_sync(section)) {
          cwr_printf(LOG_ERROR, "bad section name in pacman.conf: [%s]\n", section);
          ret = 1;
          goto finish;
        }
        cwr_printf(LOG_DEBUG, "registering alpm db: %s\n", section);
      }
    } else {
      char *key, *ptr, *token;

      key = ptr = line;
      strsep(&ptr, "=");
      strtrim(key);
      strtrim(ptr);
      if (STREQ(key, "RootDir")) {
        cwr_printf(LOG_DEBUG, "setting alpm RootDir to: %s\n", ptr);
        alpm_option_set_root(ptr);
      } else if (STREQ(key, "DBPath")) {
        cwr_printf(LOG_DEBUG, "setting alpm DBPath to: %s\n", ptr);
        alpm_option_set_dbpath(ptr);
      } else if (STREQ(key, "IgnorePkg")) {
        for (token = strtok(ptr, " "); token; token = strtok(NULL, ",")) {
          if (!alpm_list_find_str(ignore, token)) {
            cwr_printf(LOG_DEBUG, "ignoring package: %s\n", token);
            ignore = alpm_list_add(ignore, strdup(token));
          }
        }
      }
    }
  }


finish:
  free(section);
  fclose(fp);
  return(ret);
}

alpm_list_t *alpm_find_foreign_pkgs() {
  alpm_list_t *i, *ret = NULL;

  for(i = alpm_db_get_pkgcache(db_local); i; i = alpm_list_next(i)) {
    pmpkg_t *pkg = alpm_list_getdata(i);

    if (alpm_pkg_is_foreign(pkg)) {
      ret = alpm_list_add(ret, strdup(alpm_pkg_get_name(pkg)));
    }
  }

  return(ret);
}

#ifndef _HAVE_ALPM_FIND_SATISFIER
/* this is a half assed version of the real thing */
char *alpm_find_satisfier(alpm_list_t *pkgs, const char *depstring) {
  char *pkgname;
  alpm_list_t *target = NULL, *results;

  (void)pkgs; /* NOOP for compatibility */

  pkgname = strdup(depstring);
  *(pkgname + strcspn(depstring, "<>=")) = '\0';

  target = alpm_list_add(target, pkgname);

  if ((results = alpm_db_search(db_local, target))) {
    return(alpm_list_getdata(results));
  }

  return(NULL);
}
#endif

alpm_list_t *alpm_list_mmerge_dedupe(alpm_list_t *left, alpm_list_t *right, alpm_list_fn_cmp fn, alpm_list_fn_free fnf) {
  alpm_list_t *lp, *newlist;
  int compare;

  if (left == NULL) {
    return(right);
  }

  if (right == NULL) {
    return(left);
  }

  do {
    compare = fn(left->data, right->data);
    if (compare > 0) {
      newlist = right;
      right = right->next;
    } else if (compare < 0) {
      newlist = left;
      left = left->next;
    } else {
      left = alpm_list_remove_item(left, fnf);
    }
  } while (compare == 0);

  newlist->prev = NULL;
  newlist->next = NULL;
  lp = newlist;

  while ((left != NULL) && (right != NULL)) {
    compare = fn(left->data, right->data);
    if (compare < 0) {
      lp->next = left;
      left->prev = lp;
      left = left->next;
    }
    else if (compare > 0) {
      lp->next = right;
      right->prev = lp;
      right = right->next;
    } else {
      left = alpm_list_remove_item(left, fnf);
      continue;
    }
    lp = lp->next;
    lp->next = NULL;
  }
  if (left != NULL) {
    lp->next = left;
    left->prev = lp;
  }
  else if (right != NULL) {
    lp->next = right;
    right->prev = lp;
  }

  while(lp && lp->next) {
    lp = lp->next;
  }
  newlist->prev = lp;

  return(newlist);
}

alpm_list_t *alpm_list_remove_item(alpm_list_t *target, alpm_list_fn_free fn) {
  alpm_list_t *next, *listhead;

  listhead = target->next;
  if (listhead) {
    listhead->prev = target->prev;
  }

  target->prev = NULL;
  next = target->next;

  if (target->data) {
    fn(target->data);
  }
  free(target);

  return(next);
}

int alpm_pkg_is_foreign(pmpkg_t *pkg) {
  const char *pkgname;
  alpm_list_t *i;

  pkgname = alpm_pkg_get_name(pkg);

  for (i = alpm_option_get_syncdbs(); i; i = alpm_list_next(i)) {
    if (alpm_db_get_pkg(alpm_list_getdata(i), pkgname)) {
      return(0);
    }
  }

  return(1);
}

pmdb_t *alpm_provides_pkg(const char *pkgname) {
  alpm_list_t *i;
  pmdb_t *db;

  for (i = alpm_option_get_syncdbs(); i; i = alpm_list_next(i)) {
    db = alpm_list_getdata(i);
    if (alpm_db_get_pkg(db, pkgname) != NULL) {
      return(db);
    }
  }

  return(NULL);
}

int archive_extract_file(const struct response_t *file) {
  struct archive *archive;
  struct archive_entry *entry;
  const int archive_flags = ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_TIME;
  int ret = ARCHIVE_OK;

  archive = archive_read_new();
  archive_read_support_compression_all(archive);
  archive_read_support_format_all(archive);

  ret = archive_read_open_memory(archive, file->data, file->size);
  if (ret == ARCHIVE_OK) {
    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
      switch (archive_read_extract(archive, entry, archive_flags)) {
        case ARCHIVE_OK:
        case ARCHIVE_WARN:
        case ARCHIVE_RETRY:
          break;
        case ARCHIVE_FATAL:
          ret = ARCHIVE_FATAL;
          break;
        case ARCHIVE_EOF:
          ret = ARCHIVE_EOF;
          break;
      }
    }
    archive_read_close(archive);
  }

  archive_read_finish(archive);

  return(ret);
}

int aurpkg_cmp(const void *p1, const void *p2) {
  struct aurpkg_t *pkg1 = (struct aurpkg_t*)p1;
  struct aurpkg_t *pkg2 = (struct aurpkg_t*)p2;

  return(strcmp((const char*)pkg1->name, (const char*)pkg2->name));
}

void aurpkg_free(void *pkg) {
  struct aurpkg_t *it;

  if (!pkg) {
    return;
  }

  it = (struct aurpkg_t*)pkg;

  FREE(it->id);
  FREE(it->name);
  FREE(it->ver);
  FREE(it->desc);
  FREE(it->url);
  FREE(it->urlpath);
  FREE(it->lic);
  FREE(it->votes);

  FREELIST(it->depends);
  FREELIST(it->makedepends);
  FREELIST(it->optdepends);
  FREELIST(it->provides);
  FREELIST(it->conflicts);
  FREELIST(it->replaces);

  FREE(it);
}

struct aurpkg_t *aurpkg_new() {
  struct aurpkg_t *pkg;

  pkg = calloc(1, sizeof *pkg);

  pkg->cat = pkg->ood = 0;
  pkg->name = pkg->id = pkg->ver = pkg->desc = pkg->lic = pkg->url =  NULL;

  pkg->depends = pkg->makedepends = pkg->optdepends = NULL;
  pkg->provides = pkg->conflicts = pkg->replaces = NULL;

  return(pkg);
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

int cwr_vfprintf(FILE *stream, loglevel_t level, const char *format, va_list args) {
  int ret = 0;

  if (!(logmask & level)) {
    return(ret);
  }

  switch(level) {
    case LOG_INFO:
      fprintf(stream, "%s ", colstr->info);
      break;
    case LOG_ERROR:
      fprintf(stream, "%s ", colstr->error);
      break;
    case LOG_WARN:
      fprintf(stream, "%s ", colstr->warn);
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

CURL *curl_create_easy_handle() {
  CURL *handle = curl_easy_init();
  if (!handle) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: failed to create handle\n");
    return(NULL);
  }

  curl_easy_setopt(handle, CURLOPT_USERAGENT, COWER_USERAGENT);
  curl_easy_setopt(handle, CURLOPT_ENCODING, "deflate, gzip");

  return(handle);
}

char *curl_get_url_as_buffer(const char *url) {
  CURL *curl;
  CURLcode curlstat;
  long httpcode;

  struct response_t response;
  response.data = NULL;
  response.size = 0;

  curl = curl_create_easy_handle();
  if (!curl) {
    return(NULL);
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  curlstat = curl_easy_perform(curl);
  if (curlstat != CURLE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: %s\n", curl_easy_strerror(curlstat));
    goto finish;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);

  switch (httpcode) {
    case 200:
    case 404:
      break;
    default:
      cwr_fprintf(stderr, LOG_ERROR, "curl: server responded with http%ld\n",
          httpcode);
  }

finish:
  curl_easy_cleanup(curl);
  return(response.data);
}

size_t curl_write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
  size_t realsize = size * nmemb;
  struct response_t *mem = (struct response_t*)stream;

  mem->data = realloc(mem->data, mem->size + realsize + 1);
  if (mem->data) {
    memcpy(&(mem->data[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
  }

  return(realsize);
}

int file_exists(const char *path) {
  struct stat st;
  return(stat(path, &st));
}

int getcols() {
  struct winsize win;

  if(!isatty(fileno(stdin))) {
    return(80);
  }

  if(ioctl(1, TIOCGWINSZ, &win) == 0) {
    return(win.ws_col);
  }

  return(0);
}

char *get_file_as_buffer(const char *path) {
  FILE *fp;
  char *buf;
  long fsize, nread;

  fp = fopen(path, "r");
  if (!fp) {
    cwr_fprintf(stderr, LOG_ERROR, "fopen: %s\n", strerror(errno));
    return(NULL);
  }

  fseek(fp, 0L, SEEK_END);
  fsize = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  buf = calloc(1, fsize + 1);
  if (!buf) {
    cwr_fprintf(stderr, LOG_ERROR, "Failed to allocate %ld bytes\n", fsize + 1);
    return(NULL);
  }

  nread = fread(buf, 1, fsize, fp);
  fclose(fp);

  if (nread < fsize) {
    cwr_fprintf(stderr, LOG_ERROR, "Failed to read full PKGBUILD\n");
    return(NULL);
  }

  return(buf);
}

void indentprint(const char *str, int indent) {
  wchar_t *wcstr;
  const wchar_t *p;
  int len, cidx, cols;

  if(!str) {
    return;
  }

  cols = getcols();

  /* if we're not a tty, print without indenting */
  if(cols == 0) {
    printf("%s", str);
    return;
  }

  len = strlen(str) + 1;
  wcstr = calloc(len, sizeof *wcstr);
  len = mbstowcs(wcstr, str, len);
  p = wcstr;
  cidx = indent;

  if(!p || !len) {
    return;
  }

  while(*p) {
    if(*p == L' ') {
      const wchar_t *q, *next;
      p++;
      if (p == NULL || *p == L' ') {
        continue;
      }
      next = wcschr(p, L' ');
      if (next == NULL) {
        next = p + wcslen(p);
      }

      /* len captures # cols */
      len = 0;
      q = p;

      while (q < next) {
        len += wcwidth(*q++);
      }

      if(len > (cols - cidx - 1)) {
        /* wrap to a newline and reindent */
        printf("\n%-*s", indent, "");
        cidx = indent;
      } else {
        printf(" ");
        cidx++;
      }
      continue;
    }
    printf("%lc", (wint_t)*p);
    cidx += wcwidth(*p);
    p++;
  }
  free(wcstr);
}

int json_end_map(void *ctx) {
  struct yajl_parser_t *parse_struct = (struct yajl_parser_t*)ctx;

  if (!--(parse_struct->json_depth)) {
    aurpkg_free(parse_struct->aurpkg);
    return(0);
  }

  parse_struct->pkglist = alpm_list_add_sorted(parse_struct->pkglist,
      parse_struct->aurpkg, aurpkg_cmp);

  parse_struct->aurpkg = NULL;

  return(1);
}

int json_map_key(void *ctx, const unsigned char *data, unsigned int size) {
  struct yajl_parser_t *parse_struct = (struct yajl_parser_t*)ctx;

  strncpy(parse_struct->curkey, (const char*)data, size);
  parse_struct->curkey[size] = '\0';

  return(1);
}

int json_start_map(void *ctx) {
  struct yajl_parser_t *parse_struct = (struct yajl_parser_t*)ctx;

  if (parse_struct->json_depth++ >= 1) {
    parse_struct->aurpkg = aurpkg_new();
  }

  return(1);
}

int json_string(void *ctx, const unsigned char *data, unsigned int size) {
  struct yajl_parser_t *parse_struct = (struct yajl_parser_t*)ctx;
  const char *val = (const char*)data;

  if(STREQ(parse_struct->curkey, AUR_QUERY_TYPE) &&
      STR_STARTS_WITH(val, AUR_QUERY_ERROR)) {
    return(0);
  }

  if (STREQ(parse_struct->curkey, AUR_ID)) {
    parse_struct->aurpkg->id = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, NAME)) {
    parse_struct->aurpkg->name = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, VERSION)) {
    parse_struct->aurpkg->ver = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_CAT)) {
    parse_struct->aurpkg->cat = atoi(val);
  } else if (STREQ(parse_struct->curkey, AUR_DESC)) {
    parse_struct->aurpkg->desc = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, URL)) {
    parse_struct->aurpkg->url = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_URLPATH)) {
    parse_struct->aurpkg->urlpath = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_LICENSE)) {
    parse_struct->aurpkg->lic = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_VOTES)) {
    parse_struct->aurpkg->votes = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_OOD)) {
    parse_struct->aurpkg->ood = strncmp(val, "1", 1) == 0 ? 1 : 0;
  }

  return(1);
}

alpm_list_t *parse_bash_array(alpm_list_t *deplist, char **array, int type) {
  char *token;

  if (!*array) {
    return(NULL);
  }

  if (type == PKGDETAIL_OPTDEPENDS) {
    for (token = strtok(*array, "\\\'\"\n"); token; token = strtok(NULL, "\\\'\"\n")) {
      strtrim(token);
      if (strlen(token)) {
        cwr_printf(LOG_DEBUG, "adding depend: %s\n", token);
        deplist = alpm_list_add(deplist, strdup(token));
      }
    }
    return(deplist);
  }

  for (token = strtok(*array, " \n"); token; token = strtok(NULL, " \n")) {
    char *ptr;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    strtrim(token);
    if (strchr("\'\"", *token)) {
      token++;
    }

    ptr = token + strlen(token) - 1;
    if (strchr("\'\"", *ptr)) {
      *ptr = '\0';
    }

    /* some people feel compelled to do insane things in PKGBUILDs. these people suck */
    if (!*token || strlen(token) < 2 || *token == '$') {
      continue;
    }

    cwr_printf(LOG_DEBUG, "adding depend: %s\n", token);

    pthread_mutex_lock(&lock);
    if (!alpm_list_find_str(deplist, token)) {
      deplist = alpm_list_add(deplist, strdup(token));
    }
    pthread_mutex_unlock(&lock);
  }

  return(deplist);
}

int parse_options(int argc, char *argv[]) {
  int opt;
  int option_index = 0;

  static struct option opts[] = {
    /* Operations */
    {"download",  no_argument,        0, 'd'},
    {"info",      no_argument,        0, 'i'},
    {"search",    no_argument,        0, 's'},
    {"update",    no_argument,        0, 'u'},

    /* Options */
    {"color",     optional_argument,  0, 'c'},
    {"debug",     no_argument,        0, OP_DEBUG},
    {"force",     no_argument,        0, 'f'},
    {"help",      no_argument,        0, 'h'},
    {"ignore",    required_argument,  0, OP_IGNORE},
    {"quiet",     no_argument,        0, 'q'},
    {"ssl",       no_argument,        0, OP_SSL},
    {"target",    required_argument,  0, 't'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "cdfhiqst:u", opts, &option_index))) {
    char *token;

    if(opt < 0) {
      break;
    }

    switch (opt) {
      /* operations */
      case 's':
        opmask |= OP_SEARCH;
        break;
      case 'u':
        opmask |= OP_UPDATE;
        break;
      case 'i':
        if (!(opmask & OP_INFO)) {
          opmask |= OP_INFO;
        } else {
          optextinfo = 1;
        }
        break;
      case 'd':
        if (!(opmask & OP_DOWNLOAD)) {
          opmask |= OP_DOWNLOAD;
        } else {
          getdepends = 1;
        }
        break;

      /* options */
      case 'c':
        if (!optarg || STREQ(optarg, "auto")) {
          if (isatty(fileno(stdout))) {
            optcolor = 1;
          } else {
            optcolor = 0;
          }
        } else if (STREQ(optarg, "always")) {
          optcolor = 1;
        } else if (STREQ(optarg, "never")) {
          optcolor = 0;
        } else {
          fprintf(stderr, "invalid argument to --color\n");
          return(1);
        }
        break;
      case 'f':
        optforce = 1;
        break;
      case 'h':
        return(1);
      case 'q':
        optquiet = 1;
        break;
      case 't':
        download_dir = strndup(optarg, PATH_MAX);
        break;
      case OP_DEBUG:
        logmask |= LOG_DEBUG;
        break;
      case OP_SSL:
        optproto = HTTPS;
        break;
      case OP_IGNORE:
        for (token = strtok(optarg, ","); token; token = strtok(NULL, ",")) {
          if (!alpm_list_find_str(ignore, token)) {
            cwr_printf(LOG_DEBUG, "ignoring package: %s\n", token);
            ignore = alpm_list_add(ignore, strdup(token));
          }
        }
        break;

      case '?':
        return(1);
      default:
        return(1);
    }
  }

  while (optind < argc) {
    cwr_fprintf(stderr, LOG_DEBUG, "adding %s to targets\n", argv[optind]);
    if (!alpm_list_find_str(targets, argv[optind])) {
      targets = alpm_list_add(targets, strdup(argv[optind]));
    }
    optind++;
  }

  return(0);
}

void pkgbuild_get_extinfo(char **pkgbuild, alpm_list_t **details[]) {
  char *lineptr, *arrayend;

  for (lineptr = *pkgbuild; lineptr; lineptr = strchr(lineptr, '\n')) {
    alpm_list_t **deplist;
    int type = 0;

    strtrim(++lineptr);
    if (*lineptr == '#' || strlen(lineptr) == 0) {
      continue;
    }

    if (STR_STARTS_WITH(lineptr, PKGBUILD_DEPENDS)) {
      deplist = details[PKGDETAIL_DEPENDS];
    } else if (STR_STARTS_WITH(lineptr, PKGBUILD_MAKEDEPENDS)) {
      deplist = details[PKGDETAIL_MAKEDEPENDS];
    } else if (STR_STARTS_WITH(lineptr, PKGBUILD_OPTDEPENDS)) {
      deplist = details[PKGDETAIL_OPTDEPENDS];
      type = PKGDETAIL_OPTDEPENDS;
    } else if (STR_STARTS_WITH(lineptr, PKGBUILD_PROVIDES)) {
      deplist = details[PKGDETAIL_PROVIDES];
    } else if (STR_STARTS_WITH(lineptr, PKGBUILD_REPLACES)) {
      deplist = details[PKGDETAIL_REPLACES];
    } else if (STR_STARTS_WITH(lineptr, PKGBUILD_CONFLICTS)) {
      deplist = details[PKGDETAIL_CONFLICTS];
    } else {
      continue;
    }

    arrayend = strchr(lineptr, ')');
    *arrayend  = '\0';

    if (deplist) {
      char *arrayptr = strchr(lineptr, '(') + 1;
      *deplist = parse_bash_array(*deplist, &arrayptr, type);
    }

    lineptr = arrayend + 1;
  }
}

void print_extinfo_list(alpm_list_t *list, const char *fieldname) {
  size_t cols, count = 0;
  alpm_list_t *i;

  if (!list) {
    return;
  }

  /* this is bad -- we're relying on cols underflowing when getcols() returns 0
   * in the case of stdout not being attached to a terminal.  Unfortunately,
   * its very convenient */
  cols = getcols() - INFO_INDENT;

  count += printf("%-*s: ", INFO_INDENT - 2, fieldname);
  for (i = list; i; i = i->next) {
    if (count + strlen(alpm_list_getdata(i)) >= cols) {
      printf("%-*s", INFO_INDENT + 1, "\n");
      count = 0;
    }
    count += printf("%s  ", (const char*)i->data);
  }
  putchar('\n');
}

void print_pkg_info(struct aurpkg_t *pkg) {
  alpm_list_t *i;

  printf(PKG_REPO "     : %saur%s\n", colstr->repo, colstr->nc);
  printf(NAME "           : %s%s%s\n", colstr->pkg, pkg->name, colstr->nc);
  printf(VERSION "        : %s%s%s\n",
      pkg->ood ? colstr->ood : colstr->utd, pkg->ver, colstr->nc);
  printf(URL "            : %s%s%s\n", colstr->url, pkg->url, colstr->nc);
  printf(PKG_AURPAGE "       : %s" AUR_PKG_URL_FORMAT "%s%s\n",
      colstr->url, optproto, pkg->id, colstr->nc);

  print_extinfo_list(pkg->depends, PKG_DEPENDS);
  print_extinfo_list(pkg->makedepends, PKG_MAKEDEPENDS);
  print_extinfo_list(pkg->provides, PKG_PROVIDES);
  print_extinfo_list(pkg->conflicts, PKG_CONFLICTS);

  if (pkg->optdepends) {
    printf(PKG_OPTDEPENDS "  : %s\n", (const char*)alpm_list_getdata(pkg->optdepends));
    for (i = pkg->optdepends->next; i; i = alpm_list_next(i)) {
      printf("%-*s%s\n", INFO_INDENT, "", (const char*)alpm_list_getdata(i));
    }
  }

  print_extinfo_list(pkg->replaces, PKG_REPLACES);

  printf(PKG_CAT "       : %s\n"
         PKG_LICENSE "        : %s\n"
         PKG_NUMVOTES "          : %s\n"
         PKG_OOD "    : %s%s%s\n"
         PKG_DESC "    : ",
         aur_cat[pkg->cat], pkg->lic, pkg->votes,
         pkg->ood ? colstr->ood : colstr->utd,
         pkg->ood ? "Yes" : "No", colstr->nc);

  indentprint(pkg->desc, INFO_INDENT);
  printf("\n\n");
}

void print_pkg_search(struct aurpkg_t *pkg) {
  if (optquiet) {
    printf("%s%s%s\n", colstr->pkg, pkg->name, colstr->nc);
  } else {
    printf("%saur/%s%s%s %s%s%s\n    ", colstr->repo, colstr->nc,
        colstr->pkg, pkg->name,
        pkg->ood ? colstr->ood : colstr->utd, pkg->ver, colstr->nc);
    indentprint(pkg->desc, SRCH_INDENT);
    putchar('\n');
  }
}

void print_results(alpm_list_t *results, void (*fn)(struct aurpkg_t*)) {
  alpm_list_t *i;

  if (!results) {
    cwr_fprintf(stderr, LOG_ERROR, "no results found\n");
  }

  for (i = results; i; i = alpm_list_next(i)) {
    fn(alpm_list_getdata(i));
  }
}

int resolve_dependencies(const char *pkgname) {
  int ret = 0;
  char *filename, *pkgbuild;
  alpm_list_t *i, *deplist = NULL;
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  void *retval;

  cwr_asprintf(&filename, "%s/PKGBUILD", pkgname);

  pkgbuild = get_file_as_buffer(filename);
  if (!pkgbuild) {
    ret = 1;
    goto finish;
  }

  alpm_list_t **pkg_details[PKGDETAIL_MAX] = {
    &deplist, &deplist, NULL, NULL, NULL, NULL
  };

  cwr_printf(LOG_DEBUG, "Parsing %s for extended info\n", filename);
  pkgbuild_get_extinfo(&pkgbuild, pkg_details);

  for (i = deplist; i; i = alpm_list_next(i)) {
    const char *depend = alpm_list_getdata(i);
    char *sanitized = strdup(depend);

    *(sanitized + strcspn(sanitized, "<>=")) = '\0';

    /* synchronize discovery of new dependencies */
    pthread_mutex_lock(&lock);
    if (!alpm_list_find_str(targets, sanitized)) {
      targets = alpm_list_add(targets, sanitized);
    } else {
      FREE(sanitized);
    }
    pthread_mutex_unlock(&lock);

    /* if we freed sanitized, we didn't add a dep */
    if (!sanitized) {
      continue;
    }

    if (alpm_find_satisfier(alpm_db_get_pkgcache(db_local), depend)) {
      cwr_printf(LOG_DEBUG, "%s is already satisified\n", depend);
    } else {
      retval = thread_download(sanitized);
      alpm_list_free_inner(retval, aurpkg_free);
      alpm_list_free(retval);
    }
  }

finish:
  FREELIST(deplist);
  free(filename);
  free(pkgbuild);

  return(ret);
}

int strings_init() {
  colstr = malloc(sizeof *colstr);
  if (!colstr) {
    cwr_fprintf(stderr, LOG_ERROR, "failed to allocate %zd bytes\n", sizeof *colstr);
    return(1);
  }

  if (optcolor) {
    colstr->error = BOLDRED "::" NC;
    colstr->warn = BOLDYELLOW "::" NC;
    colstr->info = BOLDBLUE "::" NC;
    colstr->pkg = BOLD;
    colstr->repo = BOLDMAGENTA;
    colstr->url = BOLDCYAN;
    colstr->ood = BOLDRED;
    colstr->utd = BOLDGREEN;
    colstr->nc = NC;
  } else {
    colstr->error = "error:";
    colstr->warn = "warning:";
    colstr->info = "::";
    colstr->pkg = "";
    colstr->repo = "";
    colstr->url = "";
    colstr->ood = "";
    colstr->utd = "";
    colstr->nc = "";
  }

  return 0;
}

/* borrowed from pacman */
char *strtrim(char *str) {
  char *pch = str;

  if(str == NULL || *str == '\0') {
    /* string is empty, so we're done. */
    return(str);
  }

  while(isspace((unsigned char)*pch)) {
    pch++;
  }
  if (pch != str) {
    memmove(str, pch, (strlen(pch) + 1));
  }

  /* check if there wasn't anything but whitespace in the string. */
  if (*str == '\0') {
    return(str);
  }

  pch = (str + (strlen(str) - 1));
  while(isspace((unsigned char)*pch)) {
    pch--;
  }
  *++pch = '\0';

  return(str);
}

void *thread_download(void *arg) {
  alpm_list_t *queryresult = NULL;
  char *url, *escaped;
  CURL *curl;
  CURLcode curlstat;
  long httpcode;
  const void *self;
  pmdb_t *db;
  struct response_t response;
  int ret;
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  self = (const void*)pthread_self();

  pthread_mutex_lock(&lock);
  cwr_printf(LOG_DEBUG, "thread[%p]: locking alpm mutex\n", self);
  db = alpm_provides_pkg(arg);
  cwr_printf(LOG_DEBUG, "thread[%p]: unlocking alpm mutex\n", self);
  pthread_mutex_unlock(&lock);

  if (db) {
    cwr_fprintf(stderr, LOG_WARN, "%s%s%s is available in %s%s%s\n",
        colstr->pkg, (const char*)arg, colstr->nc,
        colstr->repo, alpm_db_get_name(db), colstr->nc);
    return(NULL);
  }

  queryresult = thread_query(arg);
  if (queryresult == NULL) {
    cwr_fprintf(stderr, LOG_ERROR, "no results found for %s\n", (const char*)arg);
    return(NULL);
  }

  if (file_exists(arg) == 0 && !optforce) {
    cwr_fprintf(stderr, LOG_ERROR, "`%s/%s' already exists. Use -f to overwrite.\n",
        download_dir, (const char*)arg);
    return(NULL);
  }

  response.data = NULL;
  response.size = 0;

  curl = curl_create_easy_handle();
  if (!curl) {
    return(NULL);
  }

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_response);

  /* sanitize URL */
  escaped = curl_easy_escape(curl, arg, strlen(arg));
  cwr_asprintf(&url, AUR_PKG_URL, optproto, escaped, escaped);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_free(escaped);

  sem_wait(&sem_download);
  cwr_printf(LOG_DEBUG, "---> thread[%p]: entering critical\n", self);
  curlstat = curl_easy_perform(curl);
  cwr_printf(LOG_DEBUG, "<--- thread[%p]: leaving critical\n", self);
  sem_post(&sem_download);

  if (curlstat != CURLE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: %s\n", curl_easy_strerror(curlstat));
    goto finish;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);

  switch (httpcode) {
    case 200:
      break;
    case 404:
      cwr_fprintf(stderr, LOG_ERROR, "no results found for %s\n",
          (const char*)arg);
      goto finish;
    default:
      cwr_fprintf(stderr, LOG_ERROR, "curl: server responded with http%ld\n",
          httpcode);
  }
  cwr_printf(LOG_INFO, "%s%s%s downloaded to %s\n",
      colstr->pkg, (const char*)arg, colstr->nc, download_dir);

  ret = archive_extract_file(&response);
  if (ret != ARCHIVE_EOF && ret != ARCHIVE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "failed to extract tarball\n");
    goto finish;
  }

  if (getdepends) {
    resolve_dependencies(arg);
  }

finish:
  curl_easy_cleanup(curl);
  FREE(url);
  FREE(response.data);

  return(queryresult);
}

void *thread_query(void *arg) {
  char *escaped, *url;
  CURL *curl;
  CURLcode curlstat;
  long httpcode;
  yajl_handle yajl_hand = NULL;
  struct yajl_parser_t *parse_struct;
  alpm_list_t *pkglist = NULL;

  if ((opmask & OP_SEARCH) && strlen(arg) < 2) {
    cwr_fprintf(stderr, LOG_ERROR, "search string '%s' too short\n", (const char*)arg);
    return(NULL);
  }

  parse_struct = malloc(sizeof *parse_struct);
  parse_struct->pkglist = NULL;
  parse_struct->json_depth = 0;
  yajl_hand = yajl_alloc(&callbacks, NULL, NULL, (void*)parse_struct);

  curl = curl_create_easy_handle();
  if (!curl) {
    return(NULL);
  }

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, yajl_parse_stream);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &yajl_hand);

  /* sanitize URL */
  escaped = curl_easy_escape(curl, arg, strlen(arg));
  if (opmask & OP_SEARCH) {
    cwr_asprintf(&url, AUR_RPC_URL, optproto, AUR_QUERY_TYPE_SEARCH, escaped);
  } else {
    cwr_asprintf(&url, AUR_RPC_URL, optproto, AUR_QUERY_TYPE_INFO, escaped);
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);

  sem_wait(&sem_download); /* limit downloads to AUR_MAX_CONNECTIONS */
  cwr_printf(LOG_DEBUG, "performing curl operation on %s\n", url);
  curlstat = curl_easy_perform(curl);
  sem_post(&sem_download);

  yajl_parse_complete(yajl_hand);
  yajl_free(yajl_hand);

  if (curlstat != CURLE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: %s\n", curl_easy_strerror(curlstat));
    goto finish;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode != 200) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: server responded with http%ld\n", httpcode);
    goto finish;
  }

  /* save the embedded list before we free the carrying struct */
  pkglist = parse_struct->pkglist;

  if (!pkglist) {
    goto finish;
  }

  if (optextinfo) {
    struct aurpkg_t *aurpkg;
    char *pburl, *pkgbuild;

    aurpkg = alpm_list_getdata(pkglist);

    cwr_asprintf(&pburl, AUR_PKGBUILD_PATH, optproto, escaped, escaped);
    pkgbuild = curl_get_url_as_buffer(pburl);
    free(pburl);

    alpm_list_t **pkg_details[PKGDETAIL_MAX] = {
      &aurpkg->depends, &aurpkg->makedepends, &aurpkg->optdepends,
      &aurpkg->provides, &aurpkg->conflicts, &aurpkg->replaces
    };

    pkgbuild_get_extinfo(&pkgbuild, pkg_details);
    free(pkgbuild);
  }

finish:
  curl_easy_cleanup(curl);
  curl_free(escaped);
  FREE(parse_struct);
  FREE(url);

  return(pkglist);
}

void *thread_update(void *arg) {
  pmpkg_t *pmpkg;
  struct aurpkg_t *aurpkg;
  void *dlretval, *qretval;

  if (alpm_list_find_str(ignore, arg)) {
    return(NULL);
  }

  qretval = thread_query(arg);
  aurpkg = alpm_list_getdata(qretval);
  if (aurpkg) {
    pmpkg = alpm_db_get_pkg(db_local, arg);
    if (!pmpkg) {
      cwr_fprintf(stderr, LOG_WARN, "skipping uninstalled package %s\n",
          (const char*)arg);
      goto finish;
    }

    if (alpm_pkg_vercmp(aurpkg->ver, alpm_pkg_get_version(pmpkg)) > 0) {
      if (opmask & OP_DOWNLOAD) {
        /* we don't care about this return val, but it needs to be dealt with */
        dlretval = thread_download((void*)aurpkg->name);
        alpm_list_free_inner(dlretval, aurpkg_free);
        alpm_list_free(dlretval);
      } else {
        if (optquiet) {
          printf("%s%s%s\n", colstr->pkg, (const char*)arg, colstr->nc);
        } else {
          cwr_printf(LOG_INFO, "%s%s %s%s%s -> %s%s%s\n",
              colstr->pkg, (const char*)arg,
              colstr->ood, alpm_pkg_get_version(pmpkg), colstr->nc,
              colstr->utd, aurpkg->ver, colstr->nc);
        }
      }

      return(qretval);
    }
  }

finish:
  alpm_list_free_inner(qretval, aurpkg_free);
  alpm_list_free(qretval);
  return(NULL);
}

void usage() {
  fprintf(stderr, "cower %s\n"
      "Usage: cower <operations> [options] target...\n\n", COWER_VERSION);
  fprintf(stderr,
      " Operations:\n"
      "  -d, --download          download target(s) -- pass twice to "
                                   "download AUR dependencies\n"
      "  -i, --info              show info for target(s) -- pass twice for "
                                   "more detail\n"
      "  -s, --search            search for target(s)\n"
      "  -u, --update            check for updates against AUR -- can be combined "
                                   "with the -d flag\n\n");
  fprintf(stderr, " General options:\n"
      "  -c, --color[=WHEN]      use colored output. WHEN is `never', `always', or `auto'\n"
      "      --debug             show debug output\n"
      "  -f, --force             overwrite existing files when downloading\n"
      "  -h, --help              display this help and exit\n"
      "      --ignore <pkg>      ignore a package upgrade (can be used more than once)\n"
      "  -q, --quiet             output less\n"
      "      --ssl               create connections over https\n"
      "  -t, --target <dir>      specify an alternate download directory\n\n");
}

int verify_download_path() {
  char *real_download_dir;

  if (!(opmask & OP_DOWNLOAD)) {
    return(0);
  }

  if (!download_dir) {
    download_dir = getcwd(NULL, PATH_MAX);
  }

  real_download_dir = realpath(download_dir, NULL);
  if (!real_download_dir) {
    cwr_fprintf(stderr, LOG_ERROR, "%s: %s\n", download_dir, strerror(errno));
    return(1);
  }

  if (access(real_download_dir, W_OK) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "Cannot write to %s: %s\n",
        real_download_dir, strerror(errno));
    return(1);
  }

  if (chdir(real_download_dir) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "%s: %s\n", real_download_dir, strerror(errno));
    return(1);
  }
  cwr_printf(LOG_DEBUG, "working directory set to: %s\n", real_download_dir);

  FREE(download_dir);
  download_dir = real_download_dir;

  return(0);
}

size_t yajl_parse_stream(void *ptr, size_t size, size_t nmemb, void *stream) {
  size_t realsize = size * nmemb;

  yajl_parse(*(yajl_handle*)stream, ptr, realsize);

  return(realsize);
}

int main(int argc, char *argv[]) {
  int ret, n, req_count;
  pthread_attr_t attr;
  pthread_t *threads;
  alpm_list_t *i, *results = NULL, *thread_return = NULL;
  struct task_t {
    void *(*threadfn)(void*);
    void (*printfn)(struct aurpkg_t*);
  } task = {
    .printfn = NULL,
    .threadfn = thread_query
  };

  ret = parse_options(argc, argv);
  if (ret != 0) {
    usage();
    return(1);
  }

  ret = strings_init();
  if (ret != 0) {
    return(1);
  }

  if (!opmask) {
    cwr_fprintf(stderr, LOG_ERROR, "no operation specified (use -h for help)\n");
    return(1);
  }

  cwr_printf(LOG_DEBUG, "initializing curl\n");
  if (strcmp(optproto, HTTPS) == 0) {
    curl_global_init(CURL_GLOBAL_SSL);
  } else {
    curl_global_init(CURL_GLOBAL_NOTHING);
  }

  ret = alpm_init();
  if (ret != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "failed to initialize alpm library\n");
    goto finish;
  }

  if (verify_download_path() != 0) {
    goto finish;
  }

  if (opmask & OP_UPDATE) {
    opmask &= ~OP_SEARCH; /* hackity hack */
    if (!targets) { /* allow specifying updates */
      targets = alpm_find_foreign_pkgs();
    }
  }

  req_count = alpm_list_count(targets);
  if (req_count == 0) {
    cwr_fprintf(stderr, LOG_ERROR, "no targets specified (use -h for help)\n");
    goto finish;
  }

  threads = calloc(req_count, sizeof *threads);
  if (!threads) {
    cwr_fprintf(stderr, LOG_ERROR, "failed to allocate memory: %s\n",
        strerror(errno));
    goto finish;
  }

  sem_init(&sem_download, 0, AUR_MAX_CONNECTIONS);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  /* override task behavior */
  if (opmask & OP_UPDATE) {
    task.threadfn = thread_update;
  } else if (opmask & OP_INFO) {
    task.printfn = print_pkg_info;
  } else if (opmask & OP_SEARCH) {
    task.printfn = print_pkg_search;
  } else if (opmask & OP_DOWNLOAD) {
    task.threadfn = thread_download;
  }

  for (i = targets, n = 0; i; i = alpm_list_next(i), n++) {
    void *target = alpm_list_getdata(i);

    ret = pthread_create(&threads[n], &attr, task.threadfn, target);
    if (ret != 0) {
      cwr_fprintf(stderr, LOG_ERROR, "failed to spawn new thread: %s\n",
          strerror(ret));
      break;
    }
    cwr_printf(LOG_DEBUG, "thread[%p]: spawned with arg: %s\n",
        (void*)threads[n], (const char*)target);
  }

  for (n = 0; n < req_count; n++) {
    pthread_join(threads[n], (void**)&thread_return);
    cwr_printf(LOG_DEBUG, "thread[%p]: joined\n", (void*)threads[n]);
    results = alpm_list_mmerge_dedupe(results, thread_return, aurpkg_cmp, aurpkg_free);
  }

  free(threads);
  pthread_attr_destroy(&attr);
  sem_destroy(&sem_download);

  if (task.printfn) {
    print_results(results, task.printfn);
  }

  ret = (results == NULL);
  alpm_list_free_inner(results, aurpkg_free);
  alpm_list_free(results);

finish:
  FREE(download_dir);
  FREELIST(targets);
  FREELIST(ignore);
  FREE(colstr);

  cwr_printf(LOG_DEBUG, "releasing curl\n");
  curl_global_cleanup();

  cwr_printf(LOG_DEBUG, "releasing alpm\n");
  alpm_release();

  return(ret);
}


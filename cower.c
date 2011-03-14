/* Copyright (c) 2010-2011 Dave Reisner
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
#include <locale.h>
#include <pthread.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <wchar.h>
#include <wordexp.h>

/* external libs */
#include <alpm.h>
#include <archive.h>
#include <curl/curl.h>
#include <yajl/yajl_parse.h>

/* macros */
#define ALLOC_FAIL(s) do { cwr_fprintf(stderr, LOG_ERROR, "could not allocate %zd bytes\n", s); } while(0)
#define MALLOC(p, s, action) do { p = calloc(1, s); if(!p) { ALLOC_FAIL(s); action; } } while(0)
#define CALLOC(p, l, s, action) do { p = calloc(l, s); if(!p) { ALLOC_FAIL(s); action; } } while(0)
#define FREE(x)               do { if(x) free((void*)x); x = NULL; } while(0)
#define STREQ(x,y)            (strcmp((x),(y)) == 0)
#define STR_STARTS_WITH(x,y)  (strncmp((x),(y), strlen(y)) == 0)

#define COWER_USERAGENT       "cower/3.x"

#define AUR_PKGBUILD_PATH     "%s://aur.archlinux.org/packages/%s/PKGBUILD"
#define AUR_PKG_URL           "%s://aur.archlinux.org/packages/%s/%s.tar.gz"
#define AUR_PKG_URL_FORMAT    "%s://aur.archlinux.org/packages.php?ID="
#define AUR_RPC_URL           "%s://aur.archlinux.org/rpc.php?type=%s&arg=%s"
#define THREAD_MAX            10
#define CONNECT_TIMEOUT       10L

#define AUR_QUERY_TYPE        "type"
#define AUR_QUERY_TYPE_INFO   "info"
#define AUR_QUERY_TYPE_SEARCH "search"
#define AUR_QUERY_TYPE_MSRCH  "msearch"
#define AUR_QUERY_ERROR       "error"

#define NAME                  "Name"
#define VERSION               "Version"
#define URL                   "URL"

#define AUR_ID                "ID"
#define AUR_CAT               "CategoryID"
#define AUR_DESC              "Description"
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
#define LIST_DELIM            "  "

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

#define REGEX_OPTS            REG_ICASE|REG_EXTENDED|REG_NOSUB|REG_NEWLINE
#define REGEX_CHARS           "^.+*?$[](){}|\\"

/* enums */
typedef enum __loglevel_t {
  LOG_INFO    = 1,
  LOG_ERROR   = (1 << 1),
  LOG_WARN    = (1 << 2),
  LOG_DEBUG   = (1 << 3),
  LOG_VERBOSE = (1 << 4),
} loglevel_t;

typedef enum __operation_t {
  OP_SEARCH   = 1,
  OP_INFO     = (1 << 1),
  OP_DOWNLOAD = (1 << 2),
  OP_UPDATE   = (1 << 3),
  OP_MSEARCH  = (1 << 4)
} operation_t;

enum {
  OP_DEBUG = 1000,
  OP_FORMAT,
  OP_IGNOREPKG,
  OP_IGNOREREPO,
  OP_LISTDELIM,
  OP_NOSSL,
  OP_THREADS,
  OP_TIMEOUT
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

struct task_t {
  void *(*threadfn)(CURL*, void*);
  void (*printfn)(struct aurpkg_t*);
};

/* function declarations */
static alpm_list_t *alpm_find_foreign_pkgs(void);
static int alpm_init(void);
static int alpm_pkg_is_foreign(pmpkg_t*);
static const char *alpm_provides_pkg(const char*);
static int archive_extract_file(const struct response_t*);
static int aurpkg_cmp(const void*, const void*);
static void aurpkg_free(void*);
static struct aurpkg_t *aurpkg_new(void);
static CURL *curl_init_easy_handle(CURL*);
static char *curl_get_url_as_buffer(CURL*, const char*);
static size_t curl_write_response(void*, size_t, size_t, void*);
static int cwr_asprintf(char**, const char*, ...) __attribute__((format(printf,2,3)));
static int cwr_fprintf(FILE*, loglevel_t, const char*, ...) __attribute__((format(printf,3,4)));
static int cwr_printf(loglevel_t, const char*, ...) __attribute__((format(printf,2,3)));
static int cwr_vfprintf(FILE*, loglevel_t, const char*, va_list) __attribute__((format(printf,3,0)));
static char *get_file_as_buffer(const char*);
static int getcols(void);
static void indentprint(const char*, int);
static int json_end_map(void*);
static int json_map_key(void*, const unsigned char*, unsigned int);
static int json_start_map(void*);
static int json_string(void*, const unsigned char*, unsigned int);
static alpm_list_t *parse_bash_array(alpm_list_t*, char*, int);
static int parse_configfile(void);
static int parse_options(int, char*[]);
static void pkgbuild_get_extinfo(char*, alpm_list_t**[]);
static int print_escaped(const char*);
static void print_extinfo_list(alpm_list_t*, const char*, const char*, int);
static void print_pkg_formatted(struct aurpkg_t*);
static void print_pkg_info(struct aurpkg_t*);
static void print_pkg_search(struct aurpkg_t*);
static void print_results(alpm_list_t*, void (*)(struct aurpkg_t*));
static int resolve_dependencies(CURL*, const char*);
static int set_download_path(void);
static int strings_init(void);
static char *strtrim(char*);
static void *task_download(CURL*, void*);
static void *task_query(CURL*, void*);
static void *task_update(CURL*, void*);
static void usage(void);
static size_t yajl_parse_stream(void*, size_t, size_t, void*);

/* options */
alpm_list_t *ignored_pkgs = NULL;
alpm_list_t *ignored_repos = NULL;
char *download_dir = NULL;
int optcolor = -1;
char *optdelim = LIST_DELIM;
int optextinfo = 0;
int optforce = 0;
char *optformat = NULL;
int optgetdeps = 0;
int optmaxthreads = -1;
char *optproto = "https";
int optquiet = 0;
int opttimeout = -1;

/* variables */
struct strings_t *colstr;
pmdb_t *db_local;
alpm_list_t *targs, *targets = NULL;
loglevel_t logmask = LOG_ERROR|LOG_WARN|LOG_INFO;
operation_t opmask = 0;

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

static char const digits[] = "0123456789";
static char const printf_flags[] = "'-+ #0I";

static const char *aur_cat[] = { NULL, "None", "daemons", "devel", "editors",
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
    return ret;
  }

  ret = alpm_option_set_dbpath("/var/lib/pacman");
  if (ret != 0) {
    return ret;
  }
  cwr_printf(LOG_DEBUG, "setting alpm DBPath to: %s\n", alpm_option_get_dbpath());

#ifdef _HAVE_ALPM_DB_REGISTER_LOCAL
  db_local = alpm_db_register_local();
#else
  db_local = alpm_option_get_localdb();
#endif
  if (!db_local) {
    return 1;
  }

  fp = fopen("/etc/pacman.conf", "r");
  if (!fp) {
    return 1;
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

      if (!STREQ(section, "options") && !alpm_list_find_str(ignored_repos, section)) {
        alpm_db_register_sync(section);
        cwr_printf(LOG_DEBUG, "registering alpm db: %s\n", section);
      }
    } else {
      char *key, *token;

      key = ptr = line;
      strsep(&ptr, "=");
      strtrim(key);
      strtrim(ptr);
      if (STREQ(key, "DBPath")) {
        cwr_printf(LOG_DEBUG, "setting alpm DBPath to: %s\n", ptr);
        alpm_option_set_dbpath(ptr);
      } else if (STREQ(key, "IgnorePkg")) {
        for (token = strtok(ptr, " "); token; token = strtok(NULL, ",")) {
          if (!alpm_list_find_str(ignored_pkgs, token)) {
            cwr_printf(LOG_DEBUG, "ignoring package: %s\n", token);
            ignored_pkgs = alpm_list_add(ignored_pkgs, strdup(token));
          }
        }
      }
    }
  }

  free(section);
  fclose(fp);
  return ret;
}

alpm_list_t *alpm_find_foreign_pkgs() {
  alpm_list_t *i, *ret = NULL;

  for (i = alpm_db_get_pkgcache(db_local); i; i = alpm_list_next(i)) {
    pmpkg_t *pkg = alpm_list_getdata(i);

    if (alpm_pkg_is_foreign(pkg)) {
      ret = alpm_list_add(ret, strdup(alpm_pkg_get_name(pkg)));
    }
  }

  return ret;
}

#ifndef _HAVE_ALPM_FIND_SATISFIER
/* this is a half assed version of the real thing */
pmpkg_t *alpm_find_satisfier(alpm_list_t *pkgs, const char *depstring) {
  alpm_list_t *results, *target = NULL;
  char *pkgname;

  (void)pkgs; /* NOOP for compatibility */

  pkgname = strdup(depstring);
  *(pkgname + strcspn(depstring, "<>=")) = '\0';

  target = alpm_list_add(target, pkgname);

  if ((results = alpm_db_search(db_local, target))) {
    return alpm_list_getdata(results);
  }

  return NULL;
}
#endif

int alpm_pkg_is_foreign(pmpkg_t *pkg) {
  alpm_list_t *i;
  const char *pkgname;

  pkgname = alpm_pkg_get_name(pkg);

  for (i = alpm_option_get_syncdbs(); i; i = alpm_list_next(i)) {
    if (alpm_db_get_pkg(alpm_list_getdata(i), pkgname)) {
      return 0;
    }
  }

  return 1;
}

const char *alpm_provides_pkg(const char *pkgname) {
  alpm_list_t *i;
  pmdb_t *db;

  for (i = alpm_option_get_syncdbs(); i; i = alpm_list_next(i)) {
    db = alpm_list_getdata(i);
#ifdef _HAVE_ALPM_FIND_SATISFIER
    if (alpm_find_satisfier(alpm_db_get_pkgcache(db), pkgname)) {
#else
    if (alpm_db_get_pkg(db, pkgname)) {
#endif
      return alpm_db_get_name(db);
    }
  }

  return NULL;
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
        /* NOOP on ARCHIVE_{OK,WARN,RETRY} */
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

  return ret;
}

int aurpkg_cmp(const void *p1, const void *p2) {
  struct aurpkg_t *pkg1 = (struct aurpkg_t*)p1;
  struct aurpkg_t *pkg2 = (struct aurpkg_t*)p2;

  return strcmp((const char*)pkg1->name, (const char*)pkg2->name);
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

  CALLOC(pkg, 1, sizeof *pkg, return NULL);

  pkg->cat = pkg->ood = 0;
  pkg->name = pkg->id = pkg->ver = pkg->desc = pkg->lic = pkg->url = NULL;

  pkg->depends = pkg->makedepends = pkg->optdepends = NULL;
  pkg->provides = pkg->conflicts = pkg->replaces = NULL;

  return pkg;
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

  return ret;
}

int cwr_fprintf(FILE *stream, loglevel_t level, const char *format, ...) {
  int ret;
  va_list args;

  va_start(args, format);
  ret = cwr_vfprintf(stream, level, format, args);
  va_end(args);

  return ret;
}

int cwr_printf(loglevel_t level, const char *format, ...) {
  int ret;
  va_list args;

  va_start(args, format);
  ret = cwr_vfprintf(stdout, level, format, args);
  va_end(args);

  return ret;
}

int cwr_vfprintf(FILE *stream, loglevel_t level, const char *format, va_list args) {
  int ret = 0;

  if (!(logmask & level)) {
    return ret;
  }

  switch (level) {
    case LOG_VERBOSE:
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
  return ret;
}

CURL *curl_init_easy_handle(CURL *handle) {
  if (!handle) {
    return NULL;
  }

  curl_easy_reset(handle);
  curl_easy_setopt(handle, CURLOPT_USERAGENT, COWER_USERAGENT);
  curl_easy_setopt(handle, CURLOPT_ENCODING, "deflate, gzip");
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, opttimeout);

  /* This is required of multi-threaded apps using timeouts. See
   * curl_easy_setopt(3)
   */
  if (opttimeout > 0L) {
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
  }

  return handle;
}

char *curl_get_url_as_buffer(CURL *curl, const char *url) {
  long httpcode;
  struct response_t response;
  CURLcode curlstat;

  curl = curl_init_easy_handle(curl);

  response.data = NULL;
  response.size = 0;

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
  return response.data;
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

  return realsize;
}

int getcols() {
  struct winsize win;

  if (!isatty(fileno(stdin))) {
    return 80;
  }

  if (ioctl(1, TIOCGWINSZ, &win) == 0) {
    return win.ws_col;
  }

  return 0;
}

char *get_file_as_buffer(const char *path) {
  FILE *fp;
  char *buf;
  long fsize, nread;

  fp = fopen(path, "r");
  if (!fp) {
    cwr_fprintf(stderr, LOG_ERROR, "fopen: %s\n", strerror(errno));
    return NULL;
  }

  fseek(fp, 0L, SEEK_END);
  fsize = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  CALLOC(buf, 1, (ssize_t)fsize + 1, return NULL);

  nread = fread(buf, 1, fsize, fp);
  fclose(fp);

  if (nread < fsize) {
    cwr_fprintf(stderr, LOG_ERROR, "Failed to read full PKGBUILD\n");
    return NULL;
  }

  return buf;
}

/* borrowed from pacman */
void indentprint(const char *str, int indent) {
  wchar_t *wcstr;
  const wchar_t *p;
  int len, cidx, cols;

  if (!str) {
    return;
  }

  cols = getcols();

  /* if we're not a tty, print without indenting */
  if (cols == 0) {
    printf("%s", str);
    return;
  }

  len = strlen(str) + 1;
  CALLOC(wcstr, len, sizeof *wcstr, return);
  len = mbstowcs(wcstr, str, len);
  p = wcstr;
  cidx = indent;

  if (!p || !len) {
    return;
  }

  while (*p) {
    if (*p == L' ') {
      const wchar_t *q, *next;
      p++;
      if (!p || *p == L' ') {
        continue;
      }
      next = wcschr(p, L' ');
      if (!next) {
        next = p + wcslen(p);
      }

      /* len captures # cols */
      len = 0;
      q = p;

      while (q < next) {
        len += wcwidth(*q++);
      }

      if (len > (cols - cidx - 1)) {
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

  if (--parse_struct->json_depth > 0) {
    parse_struct->pkglist = alpm_list_add_sorted(parse_struct->pkglist,
        parse_struct->aurpkg, aurpkg_cmp);
  }

  return 1;
}

int json_map_key(void *ctx, const unsigned char *data, unsigned int size) {
  struct yajl_parser_t *parse_struct = (struct yajl_parser_t*)ctx;

  strncpy(parse_struct->curkey, (const char*)data, size);
  parse_struct->curkey[size] = '\0';

  return 1;
}

int json_start_map(void *ctx) {
  struct yajl_parser_t *parse_struct = (struct yajl_parser_t*)ctx;

  if (parse_struct->json_depth++ >= 1) {
    parse_struct->aurpkg = aurpkg_new();
  }

  return 1;
}

int json_string(void *ctx, const unsigned char *data, unsigned int size) {
  struct yajl_parser_t *parse_struct = (struct yajl_parser_t*)ctx;
  const char *val = (const char*)data;

  if (STREQ(parse_struct->curkey, AUR_QUERY_TYPE) &&
      STR_STARTS_WITH(val, AUR_QUERY_ERROR)) {
    return 1;
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
  } else if (STREQ(parse_struct->curkey, AUR_LICENSE)) {
    parse_struct->aurpkg->lic = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_VOTES)) {
    parse_struct->aurpkg->votes = strndup(val, size);
  } else if (STREQ(parse_struct->curkey, AUR_OOD)) {
    parse_struct->aurpkg->ood = strncmp(val, "1", 1) == 0 ? 1 : 0;
  }

  return 1;
}

alpm_list_t *parse_bash_array(alpm_list_t *deplist, char *array, int type) {
  char *ptr, *token;

  if (!array) {
    return NULL;
  }

  if (type == PKGDETAIL_OPTDEPENDS) {
    const char *arrayend = rawmemchr(array, '\0');
    for (ptr = token = array; token <= arrayend; token++) {
      if (*token == '\'' || *token == '\"') {
        token++;
        ptr = strchr(token, *(token - 1));
        *ptr = '\0';
      } else if (isalpha(*token)) {
        ptr = token;
        while (!isspace(*++ptr));
        *(ptr - 1) = '\0';
      } else {
        continue;
      }

      strtrim(token);
      cwr_printf(LOG_DEBUG, "adding depend: %s\n", token);
      deplist = alpm_list_add(deplist, strdup(token));

      token = ptr;
    }
    return deplist;
  }

  for (token = strtok(array, " \n"); token; token = strtok(NULL, " \n")) {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    strtrim(token);
    if (*token == '\'' || *token == '\"') {
      token++;
      ptr = strrchr(token, *(token - 1));
      if (ptr) {
        *ptr = '\0';
      }
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

  return deplist;
}

int parse_configfile() {
  char *xdg_config_home, *home, *config_path;
  char line[BUFSIZ];
  int ret = 0;
  FILE *fp;

  xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (xdg_config_home) {
    cwr_asprintf(&config_path, "%s/cower/config", xdg_config_home);
  } else {
    home = getenv("HOME");
    if (!home) {
      cwr_fprintf(stderr, LOG_ERROR, "Unable to find path to config file.\n");
      return 1;
    }
    cwr_asprintf(&config_path, "%s/.config/cower/config", getenv("HOME"));
  }

  fp = fopen(config_path, "r");
  if (!fp) {
    cwr_printf(LOG_DEBUG, "config file not found. skipping parsing");
    return 0; /* not an error, just nothing to do here */
  }

  /* don't need this anymore, get rid of it ASAP */
  free(config_path);

  while (fgets(line, BUFSIZ, fp)) {
    char *key, *val;

    strtrim(line);
    if (strlen(line) == 0 || line[0] == '#') {
      continue;
    }

    if ((val = strchr(line, '#'))) {
      *val = '\0';
    }

    key = val = line;
    strsep(&val, "=");
    strtrim(key);
    strtrim(val);

    if (val && !*val) {
      val = NULL;
    }

    cwr_printf(LOG_DEBUG, "found config option: %s => %s\n", key, val);

    /* colors are not initialized in this section, so usage of cwr_printf
     * functions is verboten unless we're using loglevel_t LOG_DEBUG */
    if (STREQ(key, "NoSSL")) {
      optproto = "http";
    } else if (STREQ(key, "IgnoreRepo")) {
      for (key = strtok(val, " "); key; key = strtok(NULL, " ")) {
        if (!alpm_list_find_str(ignored_repos, key)) {
          cwr_printf(LOG_DEBUG, "ignoring repo: %s\n", key);
          ignored_repos = alpm_list_add(ignored_repos, strdup(key));
        }
      }
    } else if (STREQ(key, "IgnorePkg")) {
      for (key = strtok(val, " "); key; key = strtok(NULL, " ")) {
        if (!alpm_list_find_str(ignored_pkgs, key)) {
          cwr_printf(LOG_DEBUG, "ignoring package: %s\n", key);
          ignored_pkgs = alpm_list_add(ignored_pkgs, strdup(key));
        }
      }
    } else if (STREQ(key, "TargetDir")) {
      if (val && !download_dir) {
        wordexp_t p;
        if (wordexp(val, &p, 0) == 0) {
          if (p.we_wordc == 1) {
            download_dir = strdup(p.we_wordv[0]);
          }
          wordfree(&p);
          /* error on relative paths */
          if (*download_dir != '/') {
            fprintf(stderr, "error: TargetDir cannot be a relative path\n");
            ret = 1;
          }
        } else {
          fprintf(stderr, "error: failed to resolve option to TargetDir\n");
          ret = 1;
        }
      }
    } else if (STREQ(key, "MaxThreads")) {
      if (val && optmaxthreads == -1) {
        optmaxthreads = strtol(val, &key, 10);
        if (*key != '\0' || optmaxthreads <= 0) {
          fprintf(stderr, "error: invalid option to MaxThreads: %s\n", val);
          ret = 1;
        }
      }
    } else if (STREQ(key, "ConnectTimeout")) {
      if (val && opttimeout == -1) {
        opttimeout = strtol(val, &key, 10);
        if (*key != '\0' || opttimeout <= 0) {
          fprintf(stderr, "error: invalid option to ConnectTimeout: %s\n", val);
          ret = 1;
        }
      }
    } else if (STREQ(key, "Color")) {
      if (optcolor == -1) {
        if (!val || STREQ(val, "auto")) {
          if (isatty(fileno(stdout))) {
            optcolor = 1;
          } else {
            optcolor = 0;
          }
        } else if (STREQ(val, "always")) {
          optcolor = 1;
        } else if (STREQ(val, "never")) {
          optcolor = 0;
        } else {
          fprintf(stderr, "invalid argument to Color\n");
          return 1;
        }
      }
    } else {
      fprintf(stderr, "ignoring unknown option: %s\n", key);
    }
    if (ret > 0) {
      goto finish;
    }
  }

finish:
  fclose(fp);
  return ret;
}

int parse_options(int argc, char *argv[]) {
  int opt, option_index = 0;

  static struct option opts[] = {
    /* Operations */
    {"download",    no_argument,        0, 'd'},
    {"info",        no_argument,        0, 'i'},
    {"msearch",     no_argument,        0, 'm'},
    {"search",      no_argument,        0, 's'},
    {"update",      no_argument,        0, 'u'},

    /* Options */
    {"color",       optional_argument,  0, 'c'},
    {"debug",       no_argument,        0, OP_DEBUG},
    {"force",       no_argument,        0, 'f'},
    {"format",      required_argument,  0, OP_FORMAT},
    {"help",        no_argument,        0, 'h'},
    {"ignore",      required_argument,  0, OP_IGNOREPKG},
    {"ignorerepo",  required_argument,  0, OP_IGNOREREPO},
    {"listdelim",   required_argument,  0, OP_LISTDELIM},
    {"nossl",       no_argument,        0, OP_NOSSL},
    {"quiet",       no_argument,        0, 'q'},
    {"target",      required_argument,  0, 't'},
    {"threads",     required_argument,  0, OP_THREADS},
    {"timeout",     required_argument,  0, OP_TIMEOUT},
    {"verbose",     no_argument,        0, 'v'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "cdfhimqst:uv", opts, &option_index)) != -1) {
    char *token;

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
          optgetdeps = 1;
        }
        break;
      case 'm':
        opmask |= OP_MSEARCH;
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
          return 1;
        }
        break;
      case 'f':
        optforce = 1;
        break;
      case 'h':
        usage();
        return 1;
      case 'q':
        optquiet = 1;
        break;
      case 't':
        download_dir = strdup(optarg);
        break;
      case 'v':
        logmask |= LOG_VERBOSE;
        break;
      case OP_DEBUG:
        logmask |= LOG_DEBUG;
        break;
      case OP_FORMAT:
        optformat = optarg;
        break;
      case OP_IGNOREPKG:
        for (token = strtok(optarg, ","); token; token = strtok(NULL, ",")) {
          if (!alpm_list_find_str(ignored_pkgs, token)) {
            cwr_printf(LOG_DEBUG, "ignoring package: %s\n", token);
            ignored_pkgs = alpm_list_add(ignored_pkgs, strdup(token));
          }
        }
        break;
      case OP_IGNOREREPO:
        for (token = strtok(optarg, ","); token; token = strtok(NULL, ",")) {
          if (!alpm_list_find_str(ignored_repos, token)) {
            cwr_printf(LOG_DEBUG, "ignoring repos: %s\n", token);
            ignored_repos = alpm_list_add(ignored_repos, strdup(token));
          }
        }
        break;
      case OP_LISTDELIM:
        optdelim = optarg;
        break;
      case OP_NOSSL:
        optproto = "http";
        break;
      case OP_THREADS:
        optmaxthreads = strtol(optarg, &token, 10);
        if (*token != '\0' || optmaxthreads <= 0) {
          fprintf(stderr, "error: invalid argument to --threads\n");
          return 1;
        }
        break;

      case OP_TIMEOUT:
        opttimeout = strtol(optarg, &token, 10);
        if (*token != '\0') {
          fprintf(stderr, "error: invalid argument to --timeout\n");
          return 1;
        }
        break;

      case '?':
        return 1;
      default:
        return 1;
    }
  }

  /* check for invalid operation combos */
  if (((opmask & OP_INFO) && (opmask & ~OP_INFO)) ||
     ((opmask & OP_SEARCH) && (opmask & ~OP_SEARCH)) ||
     ((opmask & OP_MSEARCH) && (opmask & ~OP_MSEARCH)) ||
     ((opmask & (OP_UPDATE|OP_DOWNLOAD)) && (opmask & ~(OP_UPDATE|OP_DOWNLOAD)))) {

    fprintf(stderr, "error: invalid operation\n");
    return 2;
  }

  while (optind < argc) {
    if (!alpm_list_find_str(targets, argv[optind])) {
      cwr_fprintf(stderr, LOG_DEBUG, "adding target: %s\n", argv[optind]);
      targets = alpm_list_add(targets, strdup(argv[optind]));
    }
    optind++;
  }

  return 0;
}

void pkgbuild_get_extinfo(char *pkgbuild, alpm_list_t **details[]) {
  char *lineptr;

  for (lineptr = pkgbuild; lineptr; lineptr = strchr(lineptr, '\n')) {
    char *arrayend;
    int depth = 1, type = 0;
    alpm_list_t **deplist;

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

    if (deplist) {
      char *arrayptr = strchr(lineptr, '(') + 1;
      for (arrayend = arrayptr; depth; arrayend++) {
        switch (*arrayend) {
          case ')':
            depth--;
            break;
          case '(':
            depth++;
            break;
        }
      }
      *(arrayend - 1) = '\0';
      *deplist = parse_bash_array(*deplist, arrayptr, type);
      lineptr = arrayend;
    }
  }
}

int print_escaped(const char *delim) {
  const char *f;
  int out = 0;

  for (f = delim; *f != '\0'; f++) {
    if (*f == '\\') {
      switch (*++f) {
        case '\\':
          putchar('\\');
          break;
        case '"':
          putchar('\"');
          break;
        case 'a':
          putchar('\a');
          break;
        case 'b':
          putchar('\b');
          break;
        case 'e': /* \e is nonstandard */
          putchar('\033');
          break;
        case 'n':
          putchar('\n');
          break;
        case 'r':
          putchar('\r');
          break;
        case 't':
          putchar('\t');
          break;
        case 'v':
          putchar('\v');
          break;
        ++out;
      }
    } else {
      putchar(*f);
      ++out;
    }
  }

  return(out);
}

void print_extinfo_list(alpm_list_t *list, const char *fieldname, const char *delim, int wrap) {
  alpm_list_t *next, *i;
  size_t cols, count = 0;

  if (!list) {
    return;
  }

  cols = getcols();

  if (fieldname) {
    count += printf("%-*s: ", INFO_INDENT - 2, fieldname);
  }

  for (i = list; i; i = next) {
    next = alpm_list_next(i);
    if (wrap && cols > 0 && count + strlen(alpm_list_getdata(i)) >= cols) {
      printf("%-*c", INFO_INDENT + 1, '\n');
      count = INFO_INDENT;
    }
    count += printf("%s", (const char*)alpm_list_getdata(i));
    if (next) {
      count += print_escaped(delim);
    }
  }
  putchar('\n');
}

void print_pkg_formatted(struct aurpkg_t *pkg) {
  const char *p;
  char fmt[32], buf[64];
  int len;

  for (p = optformat; *p != '\0'; p++) {
    len = 0;
    if (*p == '%') {
      len = strspn(p + 1 + len, printf_flags);
      len += strspn(p + 1 + len, digits);
      snprintf(fmt, len + 3, "%ss", p);
      fmt[len + 1] = 's';
      p += len + 1;
      switch (*p) {
        /* simple attributes */
        case 'c':
          printf(fmt, aur_cat[pkg->cat]);
          break;
        case 'd':
          printf(fmt, pkg->desc);
          break;
        case 'i':
          printf(fmt, pkg->id);
          break;
        case 'l':
          printf(fmt, pkg->lic);
          break;
        case 'n':
          printf(fmt, pkg->name);
          break;
        case 'o':
          printf(fmt, pkg->votes);
          break;
        case 'p':
          snprintf(buf, 64, AUR_PKG_URL_FORMAT "%s", optproto, pkg->id);
          printf(fmt, buf);
          break;
        case 't':
          printf(fmt, pkg->ood ? "yes" : "no");
          break;
        case 'u':
          printf(fmt, pkg->url);
          break;
        case 'v':
          printf(fmt, pkg->ver);
          break;
        /* list based attributes */
        case 'C':
          print_extinfo_list(pkg->conflicts, NULL, optdelim, 0);
          break;
        case 'D':
          print_extinfo_list(pkg->depends, NULL, optdelim, 0);
          break;
        case 'M':
          print_extinfo_list(pkg->makedepends, NULL, optdelim, 0);
          break;
        case 'O':
          print_extinfo_list(pkg->optdepends, NULL, optdelim, 0);
          break;
        case 'P':
          print_extinfo_list(pkg->provides, NULL, optdelim, 0);
          break;
        case 'R':
          print_extinfo_list(pkg->replaces, NULL, optdelim, 0);
          break;
        case '%':
          putchar('%');
          break;
        default:
          putchar('?');
          break;

      }
    } else if (*p == '\\') {
      char buf[3];
      buf[0] = *p;
      buf[1] = *++p;
      buf[2] = '\0';
      print_escaped(buf);
    } else {
      putchar(*p);
    }
  }

  return;
}

void print_pkg_info(struct aurpkg_t *pkg) {
  alpm_list_t *i;
  pmpkg_t *ipkg;

  printf(PKG_REPO "     : %saur%s\n", colstr->repo, colstr->nc);
  printf(NAME "           : %s%s%s", colstr->pkg, pkg->name, colstr->nc);
  if ((ipkg = alpm_db_get_pkg(db_local, pkg->name))) {
    const char *instcolor;
    if (alpm_pkg_vercmp(pkg->ver, alpm_pkg_get_version(ipkg)) > 0) {
      instcolor = colstr->ood;
    } else {
      instcolor = colstr->utd;
    }
    printf(" %s[%sinstalled%s]%s", colstr->url, instcolor, colstr->url, colstr->nc);
  }
  putchar('\n');

  printf(VERSION "        : %s%s%s\n",
      pkg->ood ? colstr->ood : colstr->utd, pkg->ver, colstr->nc);
  printf(URL "            : %s%s%s\n", colstr->url, pkg->url, colstr->nc);
  printf(PKG_AURPAGE "       : %s" AUR_PKG_URL_FORMAT "%s%s\n",
      colstr->url, optproto, pkg->id, colstr->nc);

  print_extinfo_list(pkg->depends, PKG_DEPENDS, LIST_DELIM, 1);
  print_extinfo_list(pkg->makedepends, PKG_MAKEDEPENDS, LIST_DELIM, 1);
  print_extinfo_list(pkg->provides, PKG_PROVIDES, LIST_DELIM, 1);
  print_extinfo_list(pkg->conflicts, PKG_CONFLICTS, LIST_DELIM, 1);

  if (pkg->optdepends) {
    printf(PKG_OPTDEPENDS "  : %s\n", (const char*)alpm_list_getdata(pkg->optdepends));
    for (i = pkg->optdepends->next; i; i = alpm_list_next(i)) {
      printf("%-*s%s\n", INFO_INDENT, "", (const char*)alpm_list_getdata(i));
    }
  }

  print_extinfo_list(pkg->replaces, PKG_REPLACES, LIST_DELIM, 1);

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
    pmpkg_t *ipkg;
    printf("%saur/%s%s%s %s%s%s (%s)", colstr->repo, colstr->nc, colstr->pkg, pkg->name,
        pkg->ood ? colstr->ood : colstr->utd, pkg->ver, colstr->nc, pkg->votes);
    if ((ipkg = alpm_db_get_pkg(db_local, pkg->name))) {
      const char *instcolor;
      if (alpm_pkg_vercmp(pkg->ver, alpm_pkg_get_version(ipkg)) > 0) {
        instcolor = colstr->ood;
      } else {
        instcolor = colstr->utd;
      }
      printf(" %s[%sinstalled%s]%s", colstr->url, instcolor, colstr->url, colstr->nc);
    }
    printf("\n    ");
    indentprint(pkg->desc, SRCH_INDENT);
    putchar('\n');
  }
}

void print_results(alpm_list_t *results, void (*printfn)(struct aurpkg_t*)) {
  alpm_list_t *i;
  struct aurpkg_t *prev = NULL;

  if (!printfn) {
    return;
  }

  if (!results && (opmask & OP_INFO)) {
    cwr_fprintf(stderr, LOG_ERROR, "no results found\n");
    return;
  }

  for (i = results; i; i = alpm_list_next(i)) {
    struct aurpkg_t *pkg = alpm_list_getdata(i);

    /* don't print duplicates */
    if (!prev || aurpkg_cmp(pkg, prev) != 0) {
      printfn(pkg);
    }
    prev = pkg;
  }
}

int resolve_dependencies(CURL *curl, const char *pkgname) {
  alpm_list_t *i, *deplist = NULL;
  char *filename, *pkgbuild;
  static pthread_mutex_t flock = PTHREAD_MUTEX_INITIALIZER;
  static pthread_mutex_t alock = PTHREAD_MUTEX_INITIALIZER;
  void *retval;

  curl = curl_init_easy_handle(curl);

  cwr_asprintf(&filename, "%s/%s/PKGBUILD", download_dir, pkgname);

  pkgbuild = get_file_as_buffer(filename);
  if (!pkgbuild) {
    return 1;
  }

  alpm_list_t **pkg_details[PKGDETAIL_MAX] = {
    &deplist, &deplist, NULL, NULL, NULL, NULL
  };

  cwr_printf(LOG_DEBUG, "Parsing %s for extended info\n", filename);
  pkgbuild_get_extinfo(pkgbuild, pkg_details);
  free(pkgbuild);
  free(filename);

  for (i = deplist; i; i = alpm_list_next(i)) {
    const char *depend = alpm_list_getdata(i);
    char *sanitized = strdup(depend);

    *(sanitized + strcspn(sanitized, "<>=")) = '\0';

    pthread_mutex_lock(&alock);
    if (!alpm_list_find_str(targets, sanitized)) {
      targets = alpm_list_add(targets, sanitized);
    } else {
      FREE(sanitized);
    }
    pthread_mutex_unlock(&alock);

    if (sanitized) {
      alpm_list_t *pkgcache = alpm_db_get_pkgcache(db_local);
      pthread_mutex_lock(&flock);
      pmpkg_t *satisfier = alpm_find_satisfier(pkgcache, depend);
      pthread_mutex_unlock(&flock);
      if (satisfier) {
        cwr_printf(LOG_DEBUG, "%s is already satisified\n", depend);
      } else {
        retval = task_download(curl, sanitized);
        alpm_list_free_inner(retval, aurpkg_free);
        alpm_list_free(retval);
      }
    }
  }

  FREELIST(deplist);

  return 0;
}

int set_download_path() {
  char *resolved;

  if (!(opmask & OP_DOWNLOAD)) {
    FREE(download_dir);
    return 0;
  }

  resolved = download_dir ? realpath(download_dir, NULL) : getcwd(NULL, 0);
  if (!resolved) {
    cwr_fprintf(stderr, LOG_ERROR, "%s: %s\n", download_dir, strerror(errno));
    FREE(download_dir);
    return 1;
  }

  free(download_dir);
  download_dir = resolved;

  if (access(download_dir, W_OK) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "cannot write to %s: %s\n",
        download_dir, strerror(errno));
    return 1;
  }

  if (chdir(download_dir) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "%s: %s\n", download_dir, strerror(errno));
    return 1;
  }

  cwr_printf(LOG_DEBUG, "working directory set to: %s\n", download_dir);

  return 0;
}

int strings_init() {
  MALLOC(colstr, sizeof *colstr, return 1);

  if (optcolor > 0) {
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

  /* guard against optdelim being something other than LIST_DELIM if optextinfo
   * and format aren't provided */
  optdelim = (optextinfo && optformat) ? optdelim : LIST_DELIM;

  return 0;
}

/* borrowed from pacman */
char *strtrim(char *str) {
  char *pch = str;

  if (!str || *str == '\0') {
    return str;
  }

  while (isspace((unsigned char)*pch)) {
    pch++;
  }
  if (pch != str) {
    memmove(str, pch, (strlen(pch) + 1));
  }

  if (*str == '\0') {
    return str;
  }

  pch = (str + (strlen(str) - 1));
  while (isspace((unsigned char)*pch)) {
    pch--;
  }
  *++pch = '\0';

  return str;
}

void *task_download(CURL *curl, void *arg) {
  alpm_list_t *queryresult = NULL;
  CURLcode curlstat;
  const char *db;
  char *url, *escaped;
  const void *self;
  int ret;
  long httpcode;
  struct response_t response;
  struct stat st;
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  curl = curl_init_easy_handle(curl);

  self = (const void*)pthread_self();

  pthread_mutex_lock(&lock);
  cwr_printf(LOG_DEBUG, "[%p]: locking alpm mutex\n", self);
  db = alpm_provides_pkg(arg);
  cwr_printf(LOG_DEBUG, "[%p]: unlocking alpm mutex\n", self);
  pthread_mutex_unlock(&lock);

  if (db) {
    cwr_fprintf(stderr, LOG_WARN, "%s%s%s is available in %s%s%s\n",
        colstr->pkg, (const char*)arg, colstr->nc,
        colstr->repo, db, colstr->nc);
    return NULL;
  }

  queryresult = task_query(curl, arg);
  if (!queryresult) {
    cwr_fprintf(stderr, LOG_ERROR, "no results found for %s\n", (const char*)arg);
    return NULL;
  }

  if (stat(arg, &st) == 0 && !optforce) {
    cwr_fprintf(stderr, LOG_ERROR, "`%s/%s' already exists. Use -f to overwrite.\n",
        download_dir, (const char*)arg);
    alpm_list_free_inner(queryresult, aurpkg_free);
    alpm_list_free(queryresult);
    return NULL;
  }

  response.data = NULL;
  response.size = 0;

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_response);

  escaped = curl_easy_escape(curl, arg, strlen(arg));
  cwr_asprintf(&url, AUR_PKG_URL, optproto, escaped, escaped);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_free(escaped);

  curlstat = curl_easy_perform(curl);

  if (curlstat != CURLE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: %s\n", curl_easy_strerror(curlstat));
    goto finish;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);

  switch (httpcode) {
    case 200:
      break;
    case 404:
      cwr_fprintf(stderr, LOG_ERROR, "bad URL for package download: `%s'\n", url);
      goto finish;
    default:
      cwr_fprintf(stderr, LOG_ERROR, "curl: server responded with http%ld\n",
          httpcode);
      goto finish;
  }
  cwr_printf(LOG_INFO, "%s%s%s downloaded to %s\n",
      colstr->pkg, (const char*)arg, colstr->nc, download_dir);

  ret = archive_extract_file(&response);
  if (ret != ARCHIVE_EOF && ret != ARCHIVE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "failed to extract tarball\n");
    goto finish;
  }

  if (optgetdeps) {
    resolve_dependencies(curl, arg);
  }

finish:
  FREE(url);
  FREE(response.data);

  return queryresult;
}

void *task_query(CURL *curl, void *arg) {
  alpm_list_t *i, *pkglist = NULL;
  CURLcode curlstat;
  struct yajl_handle_t *yajl_hand = NULL;
  regex_t regex;
  const char *argstr;
  char *escaped, *url;
  long httpcode;
  int span = 0;
  struct yajl_parser_t *parse_struct;

  curl = curl_init_easy_handle(curl);

  if ((opmask & OP_SEARCH) && regcomp(&regex, arg, REGEX_OPTS) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "invalid regex pattern: %s\n", (const char*)arg);
    return NULL;
  }

  /* find a valid chunk of search string */
  if (opmask & OP_SEARCH) {
    for (argstr = arg; *argstr; argstr++) {
      span = strcspn(argstr, REGEX_CHARS);

      /* given 'cow?', we can't include w in the search */
      if (*(argstr + span) == '?' || *(argstr + span) == '*') {
        span--;
      }

      /* a string inside [] or {} cannot be a valid span */
      if (strchr("[{", *argstr)) {
        argstr = strpbrk(argstr + span, "]}");
        continue;
      }

      if (span >= 2) {
        break;
      }
    }

    if (span < 2) {
      cwr_fprintf(stderr, LOG_ERROR, "search string '%s' too short\n", (const char*)arg);
      return NULL;
    }
  } else {
    argstr = arg;
  }

  MALLOC(parse_struct, sizeof *parse_struct, return NULL);
  parse_struct->pkglist = NULL;
  parse_struct->json_depth = 0;
  yajl_hand = yajl_alloc(&callbacks, NULL, NULL, (void*)parse_struct);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, yajl_parse_stream);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, yajl_hand);

  escaped = curl_easy_escape(curl, argstr, span);
  if (opmask & OP_SEARCH) {
    cwr_asprintf(&url, AUR_RPC_URL, optproto, AUR_QUERY_TYPE_SEARCH, escaped);
  } else if (opmask & OP_MSEARCH) {
    cwr_asprintf(&url, AUR_RPC_URL, optproto, AUR_QUERY_TYPE_MSRCH, escaped);
  } else {
    cwr_asprintf(&url, AUR_RPC_URL, optproto, AUR_QUERY_TYPE_INFO, escaped);
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);

  cwr_printf(LOG_DEBUG, "[%p]: curl_easy_perform %s\n", (void*)pthread_self(), url);
  curlstat = curl_easy_perform(curl);

  yajl_parse_complete(yajl_hand);
  yajl_free(yajl_hand);

  if (curlstat != CURLE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: %s\n", curl_easy_strerror(curlstat));
    goto finish;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode >= 300) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: server responded with http%ld\n", httpcode);
    goto finish;
  }

  /* filter and save the embedded list -- skip if no regex chars */
  if (!(opmask & OP_SEARCH)) {
    pkglist = parse_struct->pkglist;
  } else {
    if (strlen(arg) == (size_t)span) {
      cwr_printf(LOG_DEBUG, "skipping regex filtering\n");
      pkglist = parse_struct->pkglist;
    } else {
      cwr_printf(LOG_DEBUG, "filtering results with regex: %s\n", (const char*)arg);
      for (i = parse_struct->pkglist; i; i = alpm_list_next(i)) {
        struct aurpkg_t *p = alpm_list_getdata(i);
        if (regexec(&regex, p->name, 0, 0, 0) != REG_NOMATCH) {
          pkglist = alpm_list_add(pkglist, p);
        } else {
          aurpkg_free(p);
        }
      }
      alpm_list_free(parse_struct->pkglist);
    }
    regfree(&regex);
  }

  if (pkglist && optextinfo) {
    struct aurpkg_t *aurpkg;
    char *pburl, *pkgbuild;

    aurpkg = alpm_list_getdata(pkglist);

    cwr_asprintf(&pburl, AUR_PKGBUILD_PATH, optproto, escaped);
    pkgbuild = curl_get_url_as_buffer(curl, pburl);
    free(pburl);

    alpm_list_t **pkg_details[PKGDETAIL_MAX] = {
      &aurpkg->depends, &aurpkg->makedepends, &aurpkg->optdepends,
      &aurpkg->provides, &aurpkg->conflicts, &aurpkg->replaces
    };

    pkgbuild_get_extinfo(pkgbuild, pkg_details);
    free(pkgbuild);
  }

finish:
  curl_free(escaped);
  FREE(parse_struct);
  FREE(url);

  return pkglist;
}

void *task_update(CURL *curl, void *arg) {
  pmpkg_t *pmpkg;
  struct aurpkg_t *aurpkg;
  void *dlretval, *qretval;

  if (alpm_list_find_str(ignored_pkgs, arg)) {
    return NULL;
  }

  cwr_printf(LOG_VERBOSE, "Checking %s%s%s for updates...\n",
      colstr->pkg, (const char*)arg, colstr->nc);

  qretval = task_query(curl, arg);
  aurpkg = alpm_list_getdata(qretval);
  if (aurpkg) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock(&lock);
    pmpkg = alpm_db_get_pkg(db_local, arg);
    pthread_mutex_unlock(&lock);

    if (!pmpkg) {
      cwr_fprintf(stderr, LOG_WARN, "skipping uninstalled package %s\n",
          (const char*)arg);
      goto finish;
    }

    if (alpm_pkg_vercmp(aurpkg->ver, alpm_pkg_get_version(pmpkg)) > 0) {
      if (opmask & OP_DOWNLOAD) {
        /* we don't care about the return, but we do care about leaks */
        dlretval = task_download(curl, (void*)aurpkg->name);
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

      return qretval;
    }
  }

finish:
  alpm_list_free_inner(qretval, aurpkg_free);
  alpm_list_free(qretval);
  return NULL;
}

void *thread_pool(void *arg) {
  alpm_list_t *ret = NULL;
  CURL *curl;
  void *job;
  struct task_t *task;
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  task = (struct task_t*)arg;
  curl = curl_easy_init();
  if (!curl) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: failed to initialize handle\n");
    return NULL;
  }

  while (1) {
    /* try to pop off the targets list */
    pthread_mutex_lock(&lock);
    job = alpm_list_getdata(targs);
    targs = alpm_list_next(targs);
    pthread_mutex_unlock(&lock);

    /* make sure we hooked a new job */
    if (!job) {
      break;
    }

    ret = alpm_list_mmerge(ret, task->threadfn(curl, job), aurpkg_cmp);
  }

  curl_easy_cleanup(curl);

  return ret;
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
      "  -m, --msearch           show packages maintained by target(s)\n"
      "  -s, --search            search for target(s)\n"
      "  -u, --update            check for updates against AUR -- can be combined "
                                   "with the -d flag\n\n");
  fprintf(stderr, " General options:\n"
      "  -f, --force             overwrite existing files when downloading\n"
      "  -h, --help              display this help and exit\n"
      "      --ignore <pkg>      ignore a package upgrade (can be used more than once)\n"
      "      --ignorerepo <repo> ignore a binary repo (can be used more than once)\n"
      "      --nossl             do not use https connections\n"
      "  -t, --target <dir>      specify an alternate download directory\n"
      "      --threads <num>     limit number of threads created\n"
      "      --timeout <num>     specify connection timeout in seconds\n\n");
  fprintf(stderr, " Output options:\n"
      "  -c, --color[=WHEN]      use colored output. WHEN is `never', `always', or `auto'\n"
      "      --debug             show debug output\n"
      "      --format <string>   print package output according to format string\n"
      "      --listdelim <delim> change list format delimeter\n"
      "  -q, --quiet             output less\n"
      "  -v, --verbose           output more\n\n");
}

size_t yajl_parse_stream(void *ptr, size_t size, size_t nmemb, void *stream) {
  struct yajl_handle_t *hand;

  hand = (struct yajl_handle_t*)stream;
  size_t realsize = size * nmemb;

  yajl_parse(hand, ptr, realsize);

  return realsize;
}

int main(int argc, char *argv[]) {
  alpm_list_t *results = NULL, *thread_return = NULL;
  int ret, n, num_threads;
  pthread_attr_t attr;
  pthread_t *threads;
  struct task_t task = {
    .printfn = NULL,
    .threadfn = task_query
  };

  setlocale(LC_ALL, "");

  if ((ret = parse_options(argc, argv)) != 0) {
    return ret;
  }

  if (!opmask) {
    fprintf(stderr, "error: no operation specified (use -h for help)\n");
    return 1;
  }

  if ((ret = parse_configfile() != 0)) {
    return ret;
  }

  /* fallback from sentinel values */
  if (optmaxthreads == -1) {
    optmaxthreads = THREAD_MAX;
  }
  if (opttimeout == -1) {
    opttimeout = CONNECT_TIMEOUT;
  }

  if ((ret = strings_init()) != 0) {
    return ret;
  }

  if ((ret = set_download_path()) != 0) {
    goto finish;
  }

  cwr_printf(LOG_DEBUG, "initializing curl\n");
  if (STREQ(optproto, "https")) {
    ret = curl_global_init(CURL_GLOBAL_SSL);
  } else {
    ret = curl_global_init(CURL_GLOBAL_NOTHING);
  }
  if (ret != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "failed to initialize curl\n");
    goto finish;
  }

  if ((ret = alpm_init()) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "failed to initialize alpm library\n");
    goto finish;
  }

  /* allow specific updates to be provided instead of examining all foreign pkgs */
  if ((opmask & OP_UPDATE) && !targets) {
    targets = alpm_find_foreign_pkgs();
  }

  targs = targets;
  num_threads = alpm_list_count(targets);
  if (num_threads == 0) {
    fprintf(stderr, "error: no targets specified (use -h for help)\n");
    goto finish;
  } else if (num_threads > optmaxthreads) {
    num_threads = optmaxthreads;
  }

  CALLOC(threads, num_threads, sizeof *threads, goto finish);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  /* override task behavior */
  if (opmask & OP_UPDATE) {
    task.threadfn = task_update;
  } else if (opmask & OP_INFO) {
    task.printfn = optformat ? print_pkg_formatted : print_pkg_info;
  } else if (opmask & (OP_SEARCH|OP_MSEARCH)) {
    task.printfn = optformat ? print_pkg_formatted : print_pkg_search;
  } else if (opmask & OP_DOWNLOAD) {
    task.threadfn = task_download;
  }

  for (n = 0; n < num_threads; n++) {
    ret = pthread_create(&threads[n], &attr, thread_pool, &task);
    if (ret != 0) {
      cwr_fprintf(stderr, LOG_ERROR, "failed to spawn new thread: %s\n",
          strerror(ret));
      return(ret); /* we don't want to recover from this */
    }
    cwr_printf(LOG_DEBUG, "[%p]: spawned\n", (void*)threads[n]);
  }

  for (n = 0; n < num_threads; n++) {
    pthread_join(threads[n], (void**)&thread_return);
    cwr_printf(LOG_DEBUG, "[%p]: joined\n", (void*)threads[n]);
    results = alpm_list_mmerge(results, thread_return, aurpkg_cmp);
  }

  /* we need to exit with a non-zero value when:
   * a) search/info/download returns nothing
   * b) update (without download) returns something
   * this is opposing behavior, so just XOR the result on a pure update
   */
  ret = ((results == NULL) ^ !(opmask & ~OP_UPDATE));
  print_results(results, task.printfn);
  alpm_list_free_inner(results, aurpkg_free);
  alpm_list_free(results);

  free(threads);
  pthread_attr_destroy(&attr);

finish:
  FREE(download_dir);
  FREELIST(targets);
  FREELIST(ignored_pkgs);
  FREELIST(ignored_repos);
  FREE(colstr);

  cwr_printf(LOG_DEBUG, "releasing curl\n");
  curl_global_cleanup();

  cwr_printf(LOG_DEBUG, "releasing alpm\n");
  alpm_release();

  return ret;
}

/* vim: set et sw=2: */

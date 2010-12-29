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
#include <regex.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <wchar.h>

/* external libs */
#include <alpm.h>
#include <archive.h>
#include <curl/curl.h>
#include <yajl/yajl_parse.h>

/* macros */
#define FREE(x)               do { if(x) free((void*)x); x = NULL; } while(0)
#define STREQ(x,y)            (strcmp((x),(y)) == 0)
#define STR_STARTS_WITH(x,y)  (strncmp((x),(y), strlen(y)) == 0)

#define COWER_USERAGENT       "cower/3.x"

#define AUR_PKGBUILD_PATH     "https://aur.archlinux.org/packages/%s/%s/PKGBUILD"
#define AUR_PKG_URL           "https://aur.archlinux.org/packages/%s/%s.tar.gz"
#define AUR_PKG_URL_FORMAT    "https://aur.archlinux.org/packages.php?ID="
#define AUR_RPC_URL           "https://aur.archlinux.org/rpc.php?type=%s&arg=%s"
#define AUR_MAX_CONNECTIONS   10
#define THREAD_MAX            20

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
#define REGEX_CHARS           "^.+*?$[]()\\"

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
  OP_IGNORE,
  OP_THREADS
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
  void *(*threadfn)(void*);
  void (*printfn)(struct aurpkg_t*);
};

/* function declarations */
static alpm_list_t *alpm_find_foreign_pkgs(void);
static int alpm_init(void);
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
static char *get_file_as_buffer(const char*);
static int getcols(void);
static void indentprint(const char*, int);
static int json_end_map(void*);
static int json_map_key(void*, const unsigned char*, unsigned int);
static int json_start_map(void*);
static int json_string(void*, const unsigned char*, unsigned int);
static alpm_list_t *parse_bash_array(alpm_list_t*, char*, int);
static int parse_options(int, char*[]);
static void pkgbuild_get_extinfo(char*, alpm_list_t**[]);
static void print_extinfo_list(alpm_list_t*, const char*);
static void print_pkg_info(struct aurpkg_t*);
static void print_pkg_search(struct aurpkg_t*);
static void print_results(alpm_list_t*, void (*)(struct aurpkg_t*));
static int resolve_dependencies(const char*);
static int set_download_path(void);
static int strings_init(void);
static char *strtrim(char*);
static void *task_download(void*);
static void *task_query(void*);
static void *task_update(void*);
static void usage(void);
static size_t yajl_parse_stream(void*, size_t, size_t, void*);

/* options */
char *download_dir = NULL;
int optcolor = 0;
int optextinfo = 0;
int optforce = 0;
int optgetdeps = 0;
int optmaxthreads = THREAD_MAX;
int optquiet = 0;

/* variables */
struct strings_t *colstr;
pmdb_t *db_local;
alpm_list_t *targs, *ignore = NULL, *targets = NULL;
loglevel_t logmask = LOG_ERROR|LOG_WARN|LOG_INFO;
operation_t opmask = 0;
sem_t sem_download;

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

static const char *aur_cat[] = { NULL, "None", "daemons", "devel", "editors",
                                "emulators", "games", "gnome", "i18n", "kde", "lib",
                                "modules", "multimedia", "network", "office",
                                "science", "system", "x11", "xfce", "kernels" };

static const char *binary_repos[] = { "testing", "core", "extra", "community",
                                      "community-testing", "multilib", NULL };


/* function implementations */
int alpm_init() {
  int ret = 0;
  FILE *fp;
  char line[PATH_MAX];
  const char **repo;
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

  for (repo = binary_repos; *repo; repo++) {
    cwr_printf(LOG_DEBUG, "registering alpm db: %s\n", *repo);
    alpm_db_register_sync(*repo);
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

    if (line[0] != '[') {
      char *key, *token;

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

  free(section);
  fclose(fp);
  return(ret);
}

alpm_list_t *alpm_find_foreign_pkgs() {
  alpm_list_t *i, *ret = NULL;

  for (i = alpm_db_get_pkgcache(db_local); i; i = alpm_list_next(i)) {
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
  alpm_list_t *results, *target = NULL;
  char *pkgname;

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

int alpm_pkg_is_foreign(pmpkg_t *pkg) {
  alpm_list_t *i;
  const char *pkgname;

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
    if (alpm_db_get_pkg(db, pkgname)) {
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
  pkg->name = pkg->id = pkg->ver = pkg->desc = pkg->lic = pkg->url = NULL;

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
  return(ret);
}

CURL *curl_create_easy_handle() {
  CURL *handle = curl_easy_init();
  if (!handle) {
    cwr_fprintf(stderr, LOG_ERROR, "curl: failed to create handle\n");
    return(NULL);
  }

  curl_easy_setopt(handle, CURLOPT_USERAGENT, COWER_USERAGENT);
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(handle, CURLOPT_ENCODING, "deflate, gzip");

  return(handle);
}

char *curl_get_url_as_buffer(const char *url) {
  long httpcode;
  struct response_t response;
  CURL *curl;
  CURLcode curlstat;

  curl = curl_create_easy_handle();
  if (!curl) {
    return(NULL);
  }

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

int getcols() {
  struct winsize win;

  if (!isatty(fileno(stdin))) {
    return(80);
  }

  if (ioctl(1, TIOCGWINSZ, &win) == 0) {
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
  wcstr = calloc(len, sizeof *wcstr);
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

  if (STREQ(parse_struct->curkey, AUR_QUERY_TYPE) &&
      STR_STARTS_WITH(val, AUR_QUERY_ERROR)) {
    return(1);
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

  return(1);
}

alpm_list_t *parse_bash_array(alpm_list_t *deplist, char *array, int type) {
  char *ptr, *token;

  if (!array) {
    return(NULL);
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
    return(deplist);
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

  return(deplist);
}

int parse_options(int argc, char *argv[]) {
  int opt, option_index = 0;

  static struct option opts[] = {
    /* Operations */
    {"download",  no_argument,        0, 'd'},
    {"info",      no_argument,        0, 'i'},
    {"msearch",   no_argument,        0, 'm'},
    {"search",    no_argument,        0, 's'},
    {"update",    no_argument,        0, 'u'},

    /* Options */
    {"color",     optional_argument,  0, 'c'},
    {"debug",     no_argument,        0, OP_DEBUG},
    {"force",     no_argument,        0, 'f'},
    {"help",      no_argument,        0, 'h'},
    {"ignore",    required_argument,  0, OP_IGNORE},
    {"quiet",     no_argument,        0, 'q'},
    {"target",    required_argument,  0, 't'},
    {"threads",   required_argument,  0, OP_THREADS},
    {"verbose",   no_argument,        0, 'v'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "cdfhimqst:uv", opts, &option_index))) {
    char *token;

    if (opt < 0) {
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
          return(1);
        }
        break;
      case 'f':
        optforce = 1;
        break;
      case 'h':
        usage();
        return(1);
      case 'q':
        optquiet = 1;
        break;
      case 't':
        download_dir = optarg;
        break;
      case 'v':
        logmask |= LOG_VERBOSE;
        break;
      case OP_DEBUG:
        logmask |= LOG_DEBUG;
        break;
      case OP_IGNORE:
        for (token = strtok(optarg, ","); token; token = strtok(NULL, ",")) {
          if (!alpm_list_find_str(ignore, token)) {
            cwr_printf(LOG_DEBUG, "ignoring package: %s\n", token);
            ignore = alpm_list_add(ignore, strdup(token));
          }
        }
        break;
      case OP_THREADS:
        optmaxthreads = strtol(optarg, &token, 10);
        if (*token != '\0') {
          fprintf(stderr, "error: invalid argument to --threads\n");
          return(1);
        }
        break;

      case '?':
        return(1);
      default:
        return(1);
    }
  }

  /* check for invalid operation combos */
  if (((opmask & OP_INFO) && (opmask & ~OP_INFO)) ||
     ((opmask & OP_SEARCH) && (opmask & ~OP_SEARCH)) ||
     ((opmask & OP_MSEARCH) && (opmask & ~OP_MSEARCH)) ||
     ((opmask & (OP_UPDATE|OP_DOWNLOAD)) && (opmask & ~(OP_UPDATE|OP_DOWNLOAD)))) {

    fprintf(stderr, "error: invalid operation\n");
    return(2);
  }

  while (optind < argc) {
    if (!alpm_list_find_str(targets, argv[optind])) {
      cwr_fprintf(stderr, LOG_DEBUG, "adding target: %s\n", argv[optind]);
      targets = alpm_list_add(targets, strdup(argv[optind]));
    }
    optind++;
  }

  return(0);
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

void print_extinfo_list(alpm_list_t *list, const char *fieldname) {
  alpm_list_t *next, *i;
  size_t cols, count = 0;

  if (!list) {
    return;
  }

  cols = getcols();

  count += printf("%-*s: ", INFO_INDENT - 2, fieldname);
  for (i = list; i; i = next) {
    next = alpm_list_next(i);
    if (cols > 0 && count + strlen(alpm_list_getdata(i)) >= cols) {
      printf("%-*c", INFO_INDENT + 1, '\n');
      count = INFO_INDENT;
    }
    count += printf("%s", (const char*)alpm_list_getdata(i));
    if (next) {
      count += printf("  ");
    }
  }
  putchar('\n');
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
      colstr->url, pkg->id, colstr->nc);

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

int resolve_dependencies(const char *pkgname) {
  alpm_list_t *i, *deplist = NULL;
  char *filename, *pkgbuild;
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  void *retval;

  cwr_asprintf(&filename, "%s/PKGBUILD", pkgname);

  pkgbuild = get_file_as_buffer(filename);
  free(filename);

  if (!pkgbuild) {
    return(1);
  }

  alpm_list_t **pkg_details[PKGDETAIL_MAX] = {
    &deplist, &deplist, NULL, NULL, NULL, NULL
  };

  cwr_printf(LOG_DEBUG, "Parsing %s for extended info\n", filename);
  pkgbuild_get_extinfo(pkgbuild, pkg_details);
  free(pkgbuild);

  for (i = deplist; i; i = alpm_list_next(i)) {
    const char *depend = alpm_list_getdata(i);
    char *sanitized = strdup(depend);

    *(sanitized + strcspn(sanitized, "<>=")) = '\0';

    pthread_mutex_lock(&lock);
    if (!alpm_list_find_str(targets, sanitized)) {
      targets = alpm_list_add(targets, sanitized);
    } else {
      FREE(sanitized);
    }
    pthread_mutex_unlock(&lock);

    if (sanitized) {
      if (alpm_find_satisfier(alpm_db_get_pkgcache(db_local), depend)) {
        cwr_printf(LOG_DEBUG, "%s is already satisified\n", depend);
      } else {
        retval = task_download(sanitized);
        alpm_list_free_inner(retval, aurpkg_free);
        alpm_list_free(retval);
      }
    }
  }

  FREELIST(deplist);

  return(0);
}

int set_download_path() {
  char *resolved;

  if (!(opmask & OP_DOWNLOAD)) {
    download_dir = NULL;
    return(0);
  }

  resolved = download_dir ? realpath(download_dir, NULL) : getcwd(NULL, 0);
  if (!resolved) {
    cwr_fprintf(stderr, LOG_ERROR, "%s: %s\n", download_dir, strerror(errno));
    download_dir = NULL;
    return(1);
  }

  download_dir = resolved;

  if (access(download_dir, W_OK) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "cannot write to %s: %s\n",
        download_dir, strerror(errno));
    return(1);
  }

  if (chdir(download_dir) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "%s: %s\n", download_dir, strerror(errno));
    return(1);
  }

  cwr_printf(LOG_DEBUG, "working directory set to: %s\n", download_dir);

  return(0);
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

  if (!str || *str == '\0') {
    return(str);
  }

  while (isspace((unsigned char)*pch)) {
    pch++;
  }
  if (pch != str) {
    memmove(str, pch, (strlen(pch) + 1));
  }

  if (*str == '\0') {
    return(str);
  }

  pch = (str + (strlen(str) - 1));
  while (isspace((unsigned char)*pch)) {
    pch--;
  }
  *++pch = '\0';

  return(str);
}

void *task_download(void *arg) {
  alpm_list_t *queryresult = NULL;
  pmdb_t *db;
  CURL *curl;
  CURLcode curlstat;
  char *url, *escaped;
  const void *self;
  int ret;
  long httpcode;
  struct response_t response;
  struct stat st;
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

  queryresult = task_query(arg);
  if (!queryresult) {
    cwr_fprintf(stderr, LOG_ERROR, "no results found for %s\n", (const char*)arg);
    return(NULL);
  }

  if (stat(arg, &st) == 0 && !optforce) {
    cwr_fprintf(stderr, LOG_ERROR, "`%s/%s' already exists. Use -f to overwrite.\n",
        download_dir, (const char*)arg);
    alpm_list_free_inner(queryresult, aurpkg_free);
    alpm_list_free(queryresult);
    return(NULL);
  }

  curl = curl_create_easy_handle();
  if (!curl) {
    return(NULL);
  }

  response.data = NULL;
  response.size = 0;

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_response);

  escaped = curl_easy_escape(curl, arg, strlen(arg));
  cwr_asprintf(&url, AUR_PKG_URL, escaped, escaped);
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
    resolve_dependencies(arg);
  }

finish:
  curl_easy_cleanup(curl);
  FREE(url);
  FREE(response.data);

  return(queryresult);
}

void *task_query(void *arg) {
  alpm_list_t *i, *pkglist = NULL;
  CURL *curl;
  CURLcode curlstat;
  yajl_handle yajl_hand = NULL;
  regex_t regex;
  const char *argstr;
  char *escaped, *url;
  long httpcode;
  int span = 0;
  struct yajl_parser_t *parse_struct;

  /* find a valid chunk of search string */
  for (argstr = arg; *argstr; argstr++) {
    span = strcspn(argstr, REGEX_CHARS);

    /* given 'cow?', we can't include w in the search */
    if (*(argstr + span) == '?' || *(argstr + span) == '*') {
      span--;
    }

    if (span >= 2) {
      break;
    }
  }

  if ((opmask & OP_SEARCH) && span < 2) {
    cwr_fprintf(stderr, LOG_ERROR, "search string '%s' too short\n", (const char*)arg);
    return(NULL);
  }

  if ((opmask & OP_SEARCH) && regcomp(&regex, arg, REGEX_OPTS) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "invalid regex pattern: %s\n", (const char*)arg);
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

  escaped = curl_easy_escape(curl, argstr, span);
  if (opmask & OP_SEARCH) {
    cwr_asprintf(&url, AUR_RPC_URL, AUR_QUERY_TYPE_SEARCH, escaped);
  } else if (opmask & OP_MSEARCH) {
    cwr_asprintf(&url, AUR_RPC_URL, AUR_QUERY_TYPE_MSRCH, escaped);
  } else {
    cwr_asprintf(&url, AUR_RPC_URL, AUR_QUERY_TYPE_INFO, escaped);
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);

  sem_wait(&sem_download); /* limit concurrent downloads to AUR_MAX_CONNECTIONS */
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

  /* filter and save the embedded list -- skip if no regex chars */
  if (!(opmask & OP_SEARCH)) {
    pkglist = parse_struct->pkglist;
  } else {
    if (strncmp(arg, argstr, span) == 0) {
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

    cwr_asprintf(&pburl, AUR_PKGBUILD_PATH, escaped, escaped);
    pkgbuild = curl_get_url_as_buffer(pburl);
    free(pburl);

    alpm_list_t **pkg_details[PKGDETAIL_MAX] = {
      &aurpkg->depends, &aurpkg->makedepends, &aurpkg->optdepends,
      &aurpkg->provides, &aurpkg->conflicts, &aurpkg->replaces
    };

    pkgbuild_get_extinfo(pkgbuild, pkg_details);
    free(pkgbuild);
  }

finish:
  curl_easy_cleanup(curl);
  curl_free(escaped);
  FREE(parse_struct);
  FREE(url);

  return(pkglist);
}

void *task_update(void *arg) {
  pmpkg_t *pmpkg;
  struct aurpkg_t *aurpkg;
  void *dlretval, *qretval;

  if (alpm_list_find_str(ignore, arg)) {
    return(NULL);
  }

  cwr_printf(LOG_VERBOSE, "Checking %s%s%s for updates...\n",
      colstr->pkg, (const char*)arg, colstr->nc);

  qretval = task_query(arg);
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
        /* we don't care about the return, but we do care about leaks */
        dlretval = task_download((void*)aurpkg->name);
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

void *thread_pool(void *arg) {
  alpm_list_t *ret = NULL;
  void *job;
  struct task_t *task;
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  task = (struct task_t*)arg;

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

    ret = alpm_list_mmerge(ret, task->threadfn(job), aurpkg_cmp);
  }

  return(ret);
}

void usage() {
  fprintf(stderr, "cower %s\n"
      "Usage: cower <operations> [options] target...\n\n", COWER_VERSION);
  fprintf(stderr,
      " Operations:\n"
      "  -d, --download          download target(s) -- pass twice to "
                                   "download AUR dependencies\n"
      "  -m, --msearch           show packages maintained by target(s)\n"
      "  -i, --info              show info for target(s) -- pass twice for "
                                   "more detail\n"
      "  -s, --search            search for target(s)\n"
      "  -u, --update            check for updates against AUR -- can be combined "
                                   "with the -d flag\n\n");
  fprintf(stderr, " General options:\n"
      "  -f, --force             overwrite existing files when downloading\n"
      "  -h, --help              display this help and exit\n"
      "      --ignore <pkg>      ignore a package upgrade (can be used more than once)\n"
      "  -t, --target <dir>      specify an alternate download directory\n"
      "      --threads <num>     limit number of threads created\n\n");
  fprintf(stderr, " Output options:\n"
      "  -c, --color[=WHEN]      use colored output. WHEN is `never', `always', or `auto'\n"
      "      --debug             show debug output\n"
      "  -q, --quiet             output less\n"
      "  -v, --verbose           output more\n\n");
}

size_t yajl_parse_stream(void *ptr, size_t size, size_t nmemb, void *stream) {
  yajl_handle hand;
  yajl_status stat;

  hand = *(yajl_handle*)stream;
  size_t realsize = size * nmemb;

  stat = yajl_parse(hand, ptr, realsize);
  if (stat != yajl_status_ok && stat != yajl_status_insufficient_data) {
    unsigned char *str = yajl_get_error(hand, 1, ptr, realsize);
    cwr_fprintf(stderr, LOG_ERROR, "json parsing error: %s\n", str);
    yajl_free_error(hand, str);
    return(realsize);
  }

  return(realsize);
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

  if ((ret = parse_options(argc, argv)) != 0) {
    return(ret);
  }

  if (!opmask) {
    fprintf(stderr, "error: no operation specified (use -h for help)\n");
    return(1);
  }

  if ((ret = strings_init()) != 0) {
    return(ret);
  }

  if ((ret = set_download_path()) != 0) {
    goto finish;
  }

  cwr_printf(LOG_DEBUG, "initializing curl\n");
  ret = curl_global_init(CURL_GLOBAL_SSL);
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

  if (!(threads = calloc(num_threads, sizeof *threads))) {
    cwr_fprintf(stderr, LOG_ERROR, "failed to allocate memory: %s\n",
        strerror(errno));
    goto finish;
  }

  sem_init(&sem_download, 0, AUR_MAX_CONNECTIONS);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  /* override task behavior */
  if (opmask & OP_UPDATE) {
    task.threadfn = task_update;
  } else if (opmask & OP_INFO) {
    task.printfn = print_pkg_info;
  } else if (opmask & (OP_SEARCH|OP_MSEARCH)) {
    task.printfn = print_pkg_search;
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
    cwr_printf(LOG_DEBUG, "thread[%p]: spawned\n", (void*)threads[n]);
  }

  for (n = 0; n < num_threads; n++) {
    pthread_join(threads[n], (void**)&thread_return);
    cwr_printf(LOG_DEBUG, "thread[%p]: joined\n", (void*)threads[n]);
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
  sem_destroy(&sem_download);

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

/* vim: set et sw=2: */

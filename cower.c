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
#include <openssl/crypto.h>
#include <yajl/yajl_parse.h>

/* macros {{{ */
#define ALLOC_FAIL(s) do { cwr_fprintf(stderr, LOG_ERROR, "could not allocate %zd bytes\n", s); } while(0)
#define MALLOC(p, s, action) do { p = calloc(1, s); if(!p) { ALLOC_FAIL(s); action; } } while(0)
#define CALLOC(p, l, s, action) do { p = calloc(l, s); if(!p) { ALLOC_FAIL(s); action; } } while(0)
#define FREE(x)               do { if(x) free((void*)x); x = NULL; } while(0)
#define STREQ(x,y)            (strcmp((x),(y)) == 0)
#define STR_STARTS_WITH(x,y)  (strncmp((x),(y), strlen(y)) == 0)
#define NCFLAG(val, flag)     (!cfg.color && (val)) ? (flag) : ""

#define COWER_USERAGENT       "cower/3.x"

#define AUR_PKGBUILD_PATH     "%s://aur.archlinux.org/packages/%s/PKGBUILD"
#define AUR_PKG_URL           "%s://aur.archlinux.org/packages/%s/%s.tar.gz"
#define AUR_PKG_URL_FORMAT    "%s://aur.archlinux.org/packages.php?ID="
#define AUR_RPC_URL           "%s://aur.archlinux.org/rpc.php?type=%s&arg=%s"
#define THREAD_DEFAULT        10
#define TIMEOUT_DEFAULT       10L
#define UNSET                 -1

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

#define COMMENT_BEG_DELIM     "class=\"comment-header\">"
#define COMMENT_END_DELIM     "</blockquote>"

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

#define BRIEF_ERR             "E"
#define BRIEF_WARN            "W"
#define BRIEF_OK              "S"
/* }}} */

/* typedefs and objects {{{ */
typedef enum __loglevel_t {
  LOG_INFO    = 1,
  LOG_ERROR   = (1 << 1),
  LOG_WARN    = (1 << 2),
  LOG_DEBUG   = (1 << 3),
  LOG_VERBOSE = (1 << 4),
  LOG_BRIEF   = (1 << 5)
} loglevel_t;

typedef enum __operation_t {
  OP_SEARCH   = 1,
  OP_INFO     = (1 << 1),
  OP_DOWNLOAD = (1 << 2),
  OP_UPDATE   = (1 << 3),
  OP_MSEARCH  = (1 << 4)
} operation_t;

typedef enum __html_strip_state_t {
  READING_TAG,
  NOT_READING_TAG
} html_strip_state_t;


enum {
  OP_DEBUG = 1000,
  OP_FORMAT,
  OP_IGNOREPKG,
  OP_IGNOREREPO,
  OP_LISTDELIM,
  OP_NOSSL,
  OP_THREADS,
  OP_TIMEOUT,
  OP_VERSION
};

typedef enum __pkgdetail_t {
  PKGDETAIL_DEPENDS = 0,
  PKGDETAIL_MAKEDEPENDS,
  PKGDETAIL_OPTDEPENDS,
  PKGDETAIL_PROVIDES,
  PKGDETAIL_CONFLICTS,
  PKGDETAIL_REPLACES,
  PKGDETAIL_MAX
} pkgdetail_t;

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
  const char *comment;
  const char *commentheader;
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

  alpm_list_t *comments;
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

struct openssl_mutex_t {
  pthread_mutex_t *lock;
  long *lock_count;
};

struct html_character_codes_t {
  char *name_code;
  char *glyph;
};
/* }}} */

/* function prototypes {{{ */
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
static alpm_list_t *filter_results(alpm_list_t*);
static alpm_list_t *get_aur_comments(char*);
static char *get_file_as_buffer(const char*);
static int getcols(void);
static void indentprint(const char*, int);
static int json_end_map(void*);
static int json_map_key(void*, const unsigned char*, size_t);
static int json_start_map(void*);
static int json_string(void*, const unsigned char*, size_t);
static void openssl_crypto_cleanup(void);
static void openssl_crypto_init(void);
static unsigned long openssl_thread_id(void);
static void openssl_thread_cb(int, int, const char*, int);
static alpm_list_t *parse_bash_array(alpm_list_t*, char*, pkgdetail_t);
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
static int set_working_dir(void);
static int strings_init(void);
static char *strip_and_sanitize_html(char*);
static char *strreplace(const char *, const char *, const char *);
static char *strtrim(char*);
static void *task_download(CURL*, void*);
static void *task_query(CURL*, void*);
static void *task_update(CURL*, void*);
static void *thread_pool(void*);
static void usage(void);
static void version(void);
static size_t yajl_parse_stream(void*, size_t, size_t, void*);
/* }}} */

/* runtime configuration {{{ */
struct {
  char *dlpath;
  const char *delim;
  const char *format;
  const char *proto;

  operation_t opmask;
  loglevel_t logmask;

  int color;
  int extinfo;
  int force;
  int getdeps;
  int maxthreads;
  int quiet;
  int skiprepos;
  int printcomments;
  long timeout;

  alpm_list_t *targets;
  struct {
    alpm_list_t *pkgs;
    alpm_list_t *repos;
  } ignore;
} cfg; /* }}} */

/* globals {{{ */
struct strings_t *colstr;
pmdb_t *db_local;
alpm_list_t *workq;
struct openssl_mutex_t openssl_lock;

static yajl_callbacks callbacks = {
  NULL,             /* null */
  NULL,             /* boolean */
  NULL,             /* integer */
  NULL,             /* double */
  NULL,             /* number */
  json_string,      /* string */
  json_start_map,   /* start_map */
  json_map_key,     /* map_key */
  json_end_map,     /* end_map */
  NULL,             /* start_array */
  NULL              /* end_array */
};

static char const *digits = "0123456789";
static char const *printf_flags = "'-+ #0I";

static const char *aur_cat[] = { NULL, "None", "daemons", "devel", "editors",
                                "emulators", "games", "gnome", "i18n", "kde", "lib",
                                "modules", "multimedia", "network", "office",
                                "science", "system", "x11", "xfce", "kernels" };

static const struct html_character_codes_t html_character_codes[] = {
  {"&quot;", "\""},
  {"&lt;", "<"},
  {"&gt;", ">"},
  {"&ndash;", "-"},
  {"&mdash;", "--"},
  {"&lsquo;", "\'"},
  {"&rsquo;", "\'"},
  {"&ldquo;", "\""},
  {"&rdquo;", "\""},
  {"&amp;", "&"}, /* do him last just to be safe */
  {NULL, NULL}
};
/* }}} */

int alpm_init() { /* {{{ */
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

  db_local = alpm_option_get_localdb();
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
      free(section);

      section = strdup(ptr);
      section[strlen(section) - 1] = '\0';

      if (!STREQ(section, "options") && !cfg.skiprepos &&
          !alpm_list_find_str(cfg.ignore.repos, section)) {
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
          if (!alpm_list_find_str(cfg.ignore.pkgs, token)) {
            cwr_printf(LOG_DEBUG, "ignoring package: %s\n", token);
            cfg.ignore.pkgs = alpm_list_add(cfg.ignore.pkgs, strdup(token));
          }
        }
      }
    }
  }

  free(section);
  fclose(fp);
  return ret;
} /* }}} */

alpm_list_t *alpm_find_foreign_pkgs() { /* {{{ */
  const alpm_list_t *i;
  alpm_list_t *ret = NULL;

  for (i = alpm_db_get_pkgcache(db_local); i; i = alpm_list_next(i)) {
    pmpkg_t *pkg = alpm_list_getdata(i);

    if (alpm_pkg_is_foreign(pkg)) {
      ret = alpm_list_add(ret, strdup(alpm_pkg_get_name(pkg)));
    }
  }

  return ret;
} /* }}} */

int alpm_pkg_is_foreign(pmpkg_t *pkg) { /* {{{ */
  const alpm_list_t *i;
  const char *pkgname;

  pkgname = alpm_pkg_get_name(pkg);

  for (i = alpm_option_get_syncdbs(); i; i = alpm_list_next(i)) {
    if (alpm_db_get_pkg(alpm_list_getdata(i), pkgname)) {
      return 0;
    }
  }

  return 1;
} /* }}} */

const char *alpm_provides_pkg(const char *pkgname) { /* {{{ */
  const alpm_list_t *i;
  pmdb_t *db;

  for (i = alpm_option_get_syncdbs(); i; i = alpm_list_next(i)) {
    db = alpm_list_getdata(i);
    if (alpm_find_satisfier(alpm_db_get_pkgcache(db), pkgname)) {
      return alpm_db_get_name(db);
    }
  }

  return NULL;
} /* }}} */

int archive_extract_file(const struct response_t *file) { /* {{{ */
  struct archive *archive;
  struct archive_entry *entry;
  const int archive_flags = ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_TIME;
  int ok, ret = ARCHIVE_OK;

  archive = archive_read_new();
  archive_read_support_compression_all(archive);
  archive_read_support_format_all(archive);

  ret = archive_read_open_memory(archive, file->data, file->size);
  if (ret == ARCHIVE_OK) {
    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
      ok = archive_read_extract(archive, entry, archive_flags);
      /* NOOP ON ARCHIVE_{OK,WARN,RETRY} */
      if (ok == ARCHIVE_FATAL || ok == ARCHIVE_EOF) {
        ret = ok;
        break;
      }
    }
    archive_read_close(archive);
  }
  archive_read_finish(archive);

  return ret;
} /* }}} */

int aurpkg_cmp(const void *p1, const void *p2) { /* {{{ */
  struct aurpkg_t *pkg1 = (struct aurpkg_t*)p1;
  struct aurpkg_t *pkg2 = (struct aurpkg_t*)p2;

  return strcmp((const char*)pkg1->name, (const char*)pkg2->name);
} /* }}} */

void aurpkg_free(void *pkg) { /* {{{ */
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

  FREELIST(it->comments);
  FREELIST(it->depends);
  FREELIST(it->makedepends);
  FREELIST(it->optdepends);
  FREELIST(it->provides);
  FREELIST(it->conflicts);
  FREELIST(it->replaces);

  FREE(it);
} /* }}} */

struct aurpkg_t *aurpkg_new() { /* {{{ */
  struct aurpkg_t *pkg;

  CALLOC(pkg, 1, sizeof *pkg, return NULL);

  return pkg;
} /* }}} */

int cwr_asprintf(char **string, const char *format, ...) { /* {{{ */
  int ret = 0;
  va_list args;

  va_start(args, format);
  ret = vasprintf(string, format, args);
  va_end(args);

  if (ret == -1) {
    cwr_fprintf(stderr, LOG_ERROR, "failed to allocate string\n");
  }

  return ret;
} /* }}} */

int cwr_fprintf(FILE *stream, loglevel_t level, const char *format, ...) { /* {{{ */
  int ret;
  va_list args;

  va_start(args, format);
  ret = cwr_vfprintf(stream, level, format, args);
  va_end(args);

  return ret;
} /* }}} */

int cwr_printf(loglevel_t level, const char *format, ...) { /* {{{ */
  int ret;
  va_list args;

  va_start(args, format);
  ret = cwr_vfprintf(stdout, level, format, args);
  va_end(args);

  return ret;
} /* }}} */

int cwr_vfprintf(FILE *stream, loglevel_t level, const char *format, va_list args) { /* {{{ */
  const char *prefix;
  char bufout[128];

  if (!(cfg.logmask & level)) {
    return 0;
  }

  switch (level) {
    case LOG_VERBOSE:
    case LOG_INFO:
      prefix = colstr->info;
      break;
    case LOG_ERROR:
      prefix = colstr->error;
      break;
    case LOG_WARN:
      prefix = colstr->warn;
      break;
    case LOG_DEBUG:
      prefix = "debug:";
      break;
    default:
      prefix = "";
      break;
  }

  /* f.l.w.: 128 should be big enough... */
  snprintf(bufout, 128, "%s %s", prefix, format);

  return vfprintf(stream, bufout, args);
} /* }}} */

CURL *curl_init_easy_handle(CURL *handle) { /* {{{ */
  if (!handle) {
    return NULL;
  }

  curl_easy_reset(handle);
  curl_easy_setopt(handle, CURLOPT_USERAGENT, COWER_USERAGENT);
  curl_easy_setopt(handle, CURLOPT_ENCODING, "deflate, gzip");
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, cfg.timeout);

  /* This is required of multi-threaded apps using timeouts. See
   * curl_easy_setopt(3) */
  if (cfg.timeout > 0L) {
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
  }

  return handle;
} /* }}} */

char *curl_get_url_as_buffer(CURL *curl, const char *url) { /* {{{ */
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
    cwr_fprintf(stderr, LOG_ERROR, "%s: %s\n", url, curl_easy_strerror(curlstat));
    goto finish;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (!(httpcode == 200 || httpcode == 404)) {
    cwr_fprintf(stderr, LOG_ERROR, "%s: server responded with http%ld\n",
        url, httpcode);
  }

finish:
  return response.data;
} /* }}} */

size_t curl_write_response(void *ptr, size_t size, size_t nmemb, void *stream) { /* {{{ */
  size_t realsize = size * nmemb;
  struct response_t *mem = (struct response_t*)stream;

  mem->data = realloc(mem->data, mem->size + realsize + 1);
  if (mem->data) {
    memcpy(&(mem->data[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
  }

  return realsize;
} /* }}} */

alpm_list_t *filter_results(alpm_list_t *list) { /* {{{ */
  const alpm_list_t *i, *j;
  alpm_list_t *filterlist = NULL;

  if (!(cfg.opmask & OP_SEARCH)) {
    return list;
  }

  for (i = cfg.targets; i; i = alpm_list_next(i)) {
    regex_t regex;
    const char *targ = (const char*)alpm_list_getdata(i);
    filterlist = NULL;

    if (regcomp(&regex, targ, REGEX_OPTS) == 0) {
      for (j = list; j; j = alpm_list_next(j)) {
        struct aurpkg_t *pkg = alpm_list_getdata(j);
        const char *name = pkg->name;
        const char *desc = pkg->desc;

        if (regexec(&regex, name, 0, 0, 0) != REG_NOMATCH ||
            regexec(&regex, desc, 0, 0, 0) != REG_NOMATCH) {
          filterlist = alpm_list_add(filterlist, pkg);
        } else {
          aurpkg_free(pkg);
        }
      }
      regfree(&regex);
    }

    /* switcheroo */
    alpm_list_free(list);
    list = filterlist;
  }

  return alpm_list_msort(filterlist, alpm_list_count(filterlist), aurpkg_cmp);
} /* }}} */

alpm_list_t *get_aur_comments(char *buf) { /* {{{ */
  char *this_comment, *comment_beg, *comment_end;
  alpm_list_t *list = NULL;

  for (comment_beg = strstr(buf, COMMENT_BEG_DELIM);
       comment_beg;
       comment_beg = strstr(comment_end, COMMENT_BEG_DELIM)) {
    comment_end = strstr(comment_beg, COMMENT_END_DELIM);

    /* we have the beginning and ending of the comment. now strip out
     * all the html tags and save the comment: */
    comment_beg += strlen(COMMENT_BEG_DELIM);
    CALLOC(this_comment, comment_end - comment_beg + 1, sizeof(char), return NULL);
    strncpy(this_comment, comment_beg, comment_end - comment_beg);
    this_comment = strip_and_sanitize_html(this_comment);

    list = alpm_list_add(list, this_comment);
  }
  return list;
} /* }}} */


int getcols() { /* {{{ */
  int termwidth = -1;
  const int default_tty = 80;
  const int default_notty = 0;

  if(!isatty(fileno(stdout))) {
    return default_notty;
  }

#ifdef TIOCGSIZE
  struct ttysize win;
  if(ioctl(1, TIOCGSIZE, &win) == 0) {
    termwidth = win.ts_cols;
  }
#elif defined(TIOCGWINSZ)
  struct winsize win;
  if(ioctl(1, TIOCGWINSZ, &win) == 0) {
    termwidth = win.ws_col;
  }
#endif
  return termwidth <= 0 ? default_tty : termwidth;
} /* }}} */

char *get_file_as_buffer(const char *path) { /* {{{ */
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
} /* }}} */

void indentprint(const char *str, int indent) { /* {{{ */
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
} /* }}} */

int json_end_map(void *ctx) { /* {{{ */
  struct yajl_parser_t *parse_struct = (struct yajl_parser_t*)ctx;

  if (--parse_struct->json_depth > 0) {
    parse_struct->pkglist = alpm_list_add_sorted(parse_struct->pkglist,
        parse_struct->aurpkg, aurpkg_cmp);
  }

  return 1;
} /* }}} */

int json_map_key(void *ctx, const unsigned char *data, size_t size) { /* {{{ */
  struct yajl_parser_t *parse_struct = (struct yajl_parser_t*)ctx;

  strncpy(parse_struct->curkey, (const char*)data, size);
  parse_struct->curkey[size] = '\0';

  return 1;
} /* }}} */

int json_start_map(void *ctx) { /* {{{ */
  struct yajl_parser_t *parse_struct = (struct yajl_parser_t*)ctx;

  if (parse_struct->json_depth++ >= 1) {
    parse_struct->aurpkg = aurpkg_new();
  }

  return 1;
} /* }}} */

int json_string(void *ctx, const unsigned char *data, size_t size) { /* {{{ */
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
} /* }}} */

void openssl_crypto_cleanup() { /* {{{ */
  int i;

  if (!STREQ(cfg.proto, "https")) {
    return;
  }

  CRYPTO_set_locking_callback(NULL);

  for (i = 0; i < CRYPTO_num_locks(); i++) {
    pthread_mutex_destroy(&(openssl_lock.lock[i]));
  }

  OPENSSL_free(openssl_lock.lock);
  OPENSSL_free(openssl_lock.lock_count);
} /* }}} */

void openssl_crypto_init() { /* {{{ */
  int i;

  openssl_lock.lock = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
  openssl_lock.lock_count = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));
  for (i = 0; i < CRYPTO_num_locks(); i++) {
    openssl_lock.lock_count[i] = 0;
    pthread_mutex_init(&(openssl_lock.lock[i]), NULL);
  }

  CRYPTO_set_id_callback(openssl_thread_id);
  CRYPTO_set_locking_callback(openssl_thread_cb);
} /* }}} */

void openssl_thread_cb(int mode, int type, const char *file, int line) { /* {{{ */
  (void)type; (void)file; (void)line;

  if (mode & CRYPTO_LOCK) {
    pthread_mutex_lock(&(openssl_lock.lock[type]));
    openssl_lock.lock_count[type]++;
  } else {
    pthread_mutex_unlock(&(openssl_lock.lock[type]));
  }
} /* }}} */

unsigned long openssl_thread_id(void) { /* {{{ */
  unsigned long ret;

  ret = (unsigned long)pthread_self();
  return(ret);
} /* }}} */

alpm_list_t *parse_bash_array(alpm_list_t *deplist, char *array, pkgdetail_t type) { /* {{{ */
  char *ptr, *token;

  if (!array) {
    return NULL;
  }

  if (type == PKGDETAIL_OPTDEPENDS) {
    const char *arrayend = rawmemchr(array, '\0');
    for (token = array; token <= arrayend; token++) {
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

  for (token = strtok(array, " \t\n"); token; token = strtok(NULL, " \t\n")) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    /* found an embedded comment. skip to the next line */
    if (*token == '#') {
      strtok(NULL, "\n");
      continue;
    }

    /* unquote the element */
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
} /* }}} */

int parse_configfile() { /* {{{ */
  char *xdg_config_home, *home, *config_path;
  char line[PATH_MAX];
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
    cwr_printf(LOG_DEBUG, "config file not found. skipping parsing\n");
    return 0; /* not an error, just nothing to do here */
  }

  /* don't need this anymore, get rid of it ASAP */
  free(config_path);

  while (fgets(line, PATH_MAX, fp)) {
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
      cfg.proto = "http";
    } else if (STREQ(key, "IgnoreRepo")) {
      for (key = strtok(val, " "); key; key = strtok(NULL, " ")) {
        if (!alpm_list_find_str(cfg.ignore.repos, key)) {
          cwr_printf(LOG_DEBUG, "ignoring repo: %s\n", key);
          cfg.ignore.repos = alpm_list_add(cfg.ignore.repos, strdup(key));
        }
      }
    } else if (STREQ(key, "IgnorePkg")) {
      for (key = strtok(val, " "); key; key = strtok(NULL, " ")) {
        if (!alpm_list_find_str(cfg.ignore.pkgs, key)) {
          cwr_printf(LOG_DEBUG, "ignoring package: %s\n", key);
          cfg.ignore.pkgs = alpm_list_add(cfg.ignore.pkgs, strdup(key));
        }
      }
    } else if (STREQ(key, "TargetDir")) {
      if (val && !cfg.dlpath) {
        wordexp_t p;
        if (wordexp(val, &p, 0) == 0) {
          if (p.we_wordc == 1) {
            cfg.dlpath = strdup(p.we_wordv[0]);
          }
          wordfree(&p);
          /* error on relative paths */
          if (*cfg.dlpath != '/') {
            fprintf(stderr, "error: TargetDir cannot be a relative path\n");
            ret = 1;
          }
        } else {
          fprintf(stderr, "error: failed to resolve option to TargetDir\n");
          ret = 1;
        }
      }
    } else if (STREQ(key, "MaxThreads")) {
      if (val && cfg.maxthreads == UNSET) {
        cfg.maxthreads = strtol(val, &key, 10);
        if (*key != '\0' || cfg.maxthreads <= 0) {
          fprintf(stderr, "error: invalid option to MaxThreads: %s\n", val);
          ret = 1;
        }
      }
    } else if (STREQ(key, "ConnectTimeout")) {
      if (val && cfg.timeout == UNSET) {
        cfg.timeout = strtol(val, &key, 10);
        if (*key != '\0' || cfg.timeout < 0) {
          fprintf(stderr, "error: invalid option to ConnectTimeout: %s\n", val);
          ret = 1;
        }
      }
    } else if (STREQ(key, "Color")) {
      if (cfg.color == UNSET) {
        if (!val || STREQ(val, "auto")) {
          if (isatty(fileno(stdout))) {
            cfg.color = 1;
          } else {
            cfg.color = 0;
          }
        } else if (STREQ(val, "always")) {
          cfg.color = 1;
        } else if (STREQ(val, "never")) {
          cfg.color = 0;
        } else {
          fprintf(stderr, "error: invalid option to Color\n");
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
} /* }}} */

int parse_options(int argc, char *argv[]) { /* {{{ */
  int opt, option_index = 0;

  static struct option opts[] = {
    /* operations */
    {"download",    no_argument,        0, 'd'},
    {"info",        no_argument,        0, 'i'},
    {"msearch",     no_argument,        0, 'm'},
    {"search",      no_argument,        0, 's'},
    {"update",      no_argument,        0, 'u'},

    /* options */
    {"brief",       no_argument,        0, 'b'},
    {"color",       optional_argument,  0, 'c'},
    {"debug",       no_argument,        0, OP_DEBUG},
    {"force",       no_argument,        0, 'f'},
    {"format",      required_argument,  0, OP_FORMAT},
    {"help",        no_argument,        0, 'h'},
    {"ignore",      required_argument,  0, OP_IGNOREPKG},
    {"ignorerepo",  optional_argument,  0, OP_IGNOREREPO},
    {"listdelim",   required_argument,  0, OP_LISTDELIM},
    {"comments",    no_argument,        0, 'n'},
    {"nossl",       no_argument,        0, OP_NOSSL},
    {"quiet",       no_argument,        0, 'q'},
    {"target",      required_argument,  0, 't'},
    {"threads",     required_argument,  0, OP_THREADS},
    {"timeout",     required_argument,  0, OP_TIMEOUT},
    {"verbose",     no_argument,        0, 'v'},
    {"version",     no_argument,        0, 'V'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "bcdfhimnqst:uvV", opts, &option_index)) != -1) {
    char *token;

    switch (opt) {
      /* operations */
      case 's':
        cfg.opmask |= OP_SEARCH;
        break;
      case 'u':
        cfg.opmask |= OP_UPDATE;
        break;
      case 'i':
        if (!(cfg.opmask & OP_INFO)) {
          cfg.opmask |= OP_INFO;
        } else {
          cfg.extinfo = 1;
        }
        break;
      case 'd':
        if (!(cfg.opmask & OP_DOWNLOAD)) {
          cfg.opmask |= OP_DOWNLOAD;
        } else {
          cfg.getdeps = 1;
        }
        break;
      case 'm':
        cfg.opmask |= OP_MSEARCH;
        break;

      /* options */
      case 'b':
        cfg.logmask |= LOG_BRIEF;
        break;
      case 'c':
        if (!optarg || STREQ(optarg, "auto")) {
          if (isatty(fileno(stdout))) {
            cfg.color = 1;
          } else {
            cfg.color = 0;
          }
        } else if (STREQ(optarg, "always")) {
          cfg.color = 1;
        } else if (STREQ(optarg, "never")) {
          cfg.color = 0;
        } else {
          fprintf(stderr, "invalid argument to --color\n");
          return 1;
        }
        break;
      case 'f':
        cfg.force = 1;
        break;
      case 'h':
        usage();
        return 1;
      case 'n':
        cfg.printcomments = 1;
        cfg.extinfo = 1;
        break;
      case 'q':
        cfg.quiet = 1;
        break;
      case 't':
        cfg.dlpath = strdup(optarg);
        break;
      case 'v':
        cfg.logmask |= LOG_VERBOSE;
        break;
      case 'V':
        version();
        return 2;
      case OP_DEBUG:
        cfg.logmask |= LOG_DEBUG;
        break;
      case OP_FORMAT:
        cfg.format = optarg;
        break;
      case OP_IGNOREPKG:
        for (token = strtok(optarg, ","); token; token = strtok(NULL, ",")) {
          if (!alpm_list_find_str(cfg.ignore.pkgs, token)) {
            cwr_printf(LOG_DEBUG, "ignoring package: %s\n", token);
            cfg.ignore.pkgs = alpm_list_add(cfg.ignore.pkgs, strdup(token));
          }
        }
        break;
      case OP_IGNOREREPO:
        if (!optarg) {
          cfg.skiprepos = 1;
        } else {
          for (token = strtok(optarg, ","); token; token = strtok(NULL, ",")) {
            if (!alpm_list_find_str(cfg.ignore.repos, token)) {
              cwr_printf(LOG_DEBUG, "ignoring repos: %s\n", token);
              cfg.ignore.repos = alpm_list_add(cfg.ignore.repos, strdup(token));
            }
          }
        }
        break;
      case OP_LISTDELIM:
        cfg.delim = optarg;
        break;
      case OP_NOSSL:
        cfg.proto = "http";
        break;
      case OP_THREADS:
        cfg.maxthreads = strtol(optarg, &token, 10);
        if (*token != '\0' || cfg.maxthreads <= 0) {
          fprintf(stderr, "error: invalid argument to --threads\n");
          return 1;
        }
        break;

      case OP_TIMEOUT:
        cfg.timeout = strtol(optarg, &token, 10);
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

  if (!cfg.opmask) {
    return 3;
  }

  /* check for invalid operation combos */
  if (((cfg.opmask & OP_INFO) && (cfg.opmask & ~OP_INFO)) ||
     ((cfg.opmask & OP_SEARCH) && (cfg.opmask & ~OP_SEARCH)) ||
     ((cfg.opmask & OP_MSEARCH) && (cfg.opmask & ~OP_MSEARCH)) ||
     ((cfg.opmask & (OP_UPDATE|OP_DOWNLOAD)) && (cfg.opmask & ~(OP_UPDATE|OP_DOWNLOAD)))) {

    fprintf(stderr, "error: invalid operation\n");
    return 2;
  }

  while (optind < argc) {
    if (!alpm_list_find_str(cfg.targets, argv[optind])) {
      cwr_fprintf(stderr, LOG_DEBUG, "adding target: %s\n", argv[optind]);
      cfg.targets = alpm_list_add(cfg.targets, strdup(argv[optind]));
    }
    optind++;
  }

  return 0;
} /* }}} */

void pkgbuild_get_extinfo(char *pkgbuild, alpm_list_t **details[]) { /* {{{ */
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
} /* }}} */

int print_escaped(const char *delim) { /* {{{ */
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
} /* }}} */

void print_extinfo_list(alpm_list_t *list, const char *fieldname, const char *delim, int wrap) { /* {{{ */
  const alpm_list_t *next, *i;
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
} /* }}} */

void print_pkg_formatted(struct aurpkg_t *pkg) { /* {{{ */
  const char *p, *end;
  char fmt[32], buf[64];
  int len;

  end = rawmemchr(cfg.format, '\0');

  for (p = cfg.format; p < end; p++) {
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
          snprintf(buf, 64, AUR_PKG_URL_FORMAT "%s", cfg.proto, pkg->id);
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
          print_extinfo_list(pkg->conflicts, NULL, cfg.delim, 0);
          break;
        case 'D':
          print_extinfo_list(pkg->depends, NULL, cfg.delim, 0);
          break;
        case 'M':
          print_extinfo_list(pkg->makedepends, NULL, cfg.delim, 0);
          break;
        case 'O':
          print_extinfo_list(pkg->optdepends, NULL, cfg.delim, 0);
          break;
        case 'P':
          print_extinfo_list(pkg->provides, NULL, cfg.delim, 0);
          break;
        case 'R':
          print_extinfo_list(pkg->replaces, NULL, cfg.delim, 0);
          break;
        case '%':
          putchar('%');
          break;
        default:
          putchar('?');
          break;
      }
    } else if (*p == '\\') {
      char ebuf[3];
      ebuf[0] = *p;
      ebuf[1] = *++p;
      ebuf[2] = '\0';
      print_escaped(ebuf);
    } else {
      putchar(*p);
    }
  }

  return;
} /* }}} */

void print_pkg_info(struct aurpkg_t *pkg) { /* {{{ */
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
      colstr->url, cfg.proto, pkg->id, colstr->nc);

  print_extinfo_list(pkg->depends, PKG_DEPENDS, LIST_DELIM, 1);
  print_extinfo_list(pkg->makedepends, PKG_MAKEDEPENDS, LIST_DELIM, 1);
  print_extinfo_list(pkg->provides, PKG_PROVIDES, LIST_DELIM, 1);
  print_extinfo_list(pkg->conflicts, PKG_CONFLICTS, LIST_DELIM, 1);

  if (pkg->optdepends) {
    const alpm_list_t *i;
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

  if (cfg.extinfo && cfg.printcomments) {
    const alpm_list_t *i;
    if (alpm_list_count(pkg->comments) == 0) {
      printf("%sNo comments%s\n\n", colstr->commentheader, colstr->nc);
    } else {
      for (i = pkg->comments; i; i = i->next) {
        char *comment_header, *end_of_first_line, *the_comment;
        the_comment = alpm_list_getdata(i);
        end_of_first_line = strchr(the_comment, '\n');
        CALLOC(comment_header, end_of_first_line - the_comment + 1, sizeof(char), return);
        strncpy(comment_header, the_comment, end_of_first_line - the_comment);
        printf("%s%s%s", colstr->commentheader, comment_header, colstr->nc);
        printf("%s%s%s\n", colstr->comment, end_of_first_line, colstr->nc);
        free(comment_header);
      }
    }
  }

} /* }}} */

void print_pkg_search(struct aurpkg_t *pkg) { /* {{{ */
  if (cfg.quiet) {
    printf("%s%s%s\n", colstr->pkg, pkg->name, colstr->nc);
  } else {
    pmpkg_t *ipkg;
    printf("%saur/%s%s%s %s%s%s%s (%s)", colstr->repo, colstr->nc, colstr->pkg,
        pkg->name, pkg->ood ? colstr->ood : colstr->utd, pkg->ver,
        NCFLAG(pkg->ood, " <!>"), colstr->nc, pkg->votes);
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
} /* }}} */

void print_results(alpm_list_t *results, void (*printfn)(struct aurpkg_t*)) { /* {{{ */
  const alpm_list_t *i;
  struct aurpkg_t *prev = NULL;

  if (!printfn) {
    return;
  }

  if (!results && (cfg.opmask & OP_INFO)) {
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
} /* }}} */

int resolve_dependencies(CURL *curl, const char *pkgname) { /* {{{ */
  const alpm_list_t *i;
  alpm_list_t *deplist = NULL;
  char *filename, *pkgbuild;
  static pthread_mutex_t flock = PTHREAD_MUTEX_INITIALIZER;
  static pthread_mutex_t alock = PTHREAD_MUTEX_INITIALIZER;
  void *retval;

  curl = curl_init_easy_handle(curl);

  cwr_asprintf(&filename, "%s/%s/PKGBUILD", cfg.dlpath, pkgname);

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
    if (!alpm_list_find_str(cfg.targets, sanitized)) {
      cfg.targets = alpm_list_add(cfg.targets, sanitized);
    } else {
      if (cfg.logmask & LOG_BRIEF &&
              !alpm_find_satisfier(alpm_db_get_pkgcache(db_local), depend)) {
          cwr_printf(LOG_BRIEF, "S\t%s\n", sanitized);
      }
      FREE(sanitized);
    }
    pthread_mutex_unlock(&alock);

    if (sanitized) {
      pthread_mutex_lock(&flock);
      alpm_list_t *pkgcache = alpm_db_get_pkgcache(db_local);
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
} /* }}} */

int set_working_dir() { /* {{{ */
  char *resolved;

  if (!(cfg.opmask & OP_DOWNLOAD)) {
    FREE(cfg.dlpath);
    return 0;
  }

  resolved = cfg.dlpath ? realpath(cfg.dlpath, NULL) : getcwd(NULL, 0);
  if (!resolved) {
    cwr_fprintf(stderr, LOG_ERROR, "%s: %s\n", cfg.dlpath, strerror(errno));
    FREE(cfg.dlpath);
    return 1;
  }

  free(cfg.dlpath);
  cfg.dlpath = resolved;

  if (access(cfg.dlpath, W_OK) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "cannot write to %s: %s\n",
        cfg.dlpath, strerror(errno));
    FREE(cfg.dlpath);
    return 1;
  }

  if (chdir(cfg.dlpath) != 0) {
    cwr_fprintf(stderr, LOG_ERROR, "%s: %s\n", cfg.dlpath, strerror(errno));
    return 1;
  }

  cwr_printf(LOG_DEBUG, "working directory set to: %s\n", cfg.dlpath);

  return 0;
} /* }}} */

int strings_init() { /* {{{ */
  MALLOC(colstr, sizeof *colstr, return 1);

  if (cfg.color > 0) {
    colstr->error = BOLDRED "::" NC;
    colstr->warn = BOLDYELLOW "::" NC;
    colstr->info = BOLDBLUE "::" NC;
    colstr->pkg = BOLD;
    colstr->repo = BOLDMAGENTA;
    colstr->url = BOLDCYAN;
    colstr->ood = BOLDRED;
    colstr->utd = BOLDGREEN;
    colstr->nc = NC;
    colstr->commentheader = BOLDYELLOW;
    colstr->comment = "\n---";
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
    colstr->commentheader = "";
    colstr->comment = "\n---";
  }

  /* guard against delim being something other than LIST_DELIM if extinfo
   * and format aren't provided */
  cfg.delim = (cfg.extinfo && cfg.format) ? cfg.delim : LIST_DELIM;

  return 0;
} /* }}} */

/**
 * Strips html tags from input string, returning the result. Also
 * removes superfluous whitespace and replaces special html
 * characters.
 */
char *strip_and_sanitize_html(char *html) { /* {{{ */
  int i, output_ind=0, len;
  char *output;
  html_strip_state_t html_state;
  struct html_character_codes_t html_code_mapping;

  len = strlen(html);
  CALLOC(output, len+1, sizeof(char), return NULL);

  /* Finite state machine with 2 states: reading a tag or not. We
   * ignore the possibility of an html attribute containing the
   * character '>' */
  html_state = NOT_READING_TAG;
  for (i = 0; i < len; ++i) {
    if (html_state == READING_TAG) {
      if (html[i] == '>') /* end of this tag */
        html_state = NOT_READING_TAG;
    } else {
      if (html[i] == '<') /* here's a new tag */
        html_state = READING_TAG;
      else
        /* we discard two newlines in a row and all tabs */
        if (!(html[i] == '\t' || (output_ind > 0 &&
                                  output[output_ind-1] == '\n' && html[i] == '\n')))
          output[output_ind++] = html[i];
    }
  }
  output[output_ind] = '\0';
  free(html);

  /* Clean up some special html characters */
  i=0;
  for (;;) {
    html_code_mapping = html_character_codes[i++];
    if (html_code_mapping.name_code == NULL) break;

    /* avoid the mallocs and frees associated with string replace if possible: */
    if (strstr(output, html_code_mapping.name_code) != NULL) {
      char *output_before = output;
      output = strreplace(output, html_code_mapping.name_code, html_code_mapping.glyph);
      FREE(output_before);
    }
  }

  return output;
} /* }}} */

/**
 * String replace. The pointer that is returned should be free'd by
 * the user.
 */
char *strreplace(const char *st,
                 const char *replace,
                 const char *replace_with) { /* {{{ */
    char *ret, *found, *replace_, *replace_with_, *st_, *tmp, *last, *writing;
    size_t newlen;
    int len_replace, len_replace_with, len_st, cnt;

    len_replace      = strlen(replace);
    len_replace_with = strlen(replace_with);
    len_st           = strlen(st);

    MALLOC(replace_, (size_t)len_replace + 1, return NULL);
    MALLOC(replace_with_, (size_t)len_replace_with + 1, return NULL);
    MALLOC(st_, (size_t)len_st + 1, return NULL);
    strcpy(st_, st); /* what we won't do for const... */
    found         = strstr(st_, replace);
    if (found == NULL) {
        /* can't assign directly due to const: */
        ret = (char *) malloc(strlen(st_)+1);
        strcpy(ret, st_);
        goto finishup;
    }

    strcpy(replace_, replace);
    strcpy(replace_with_, replace_with);

    tmp = st_;
    for (cnt = 0; ; cnt++) {
        found = strstr(tmp, replace);
        if (found == NULL) break;
        tmp = found + len_replace;
    }

    newlen = len_st - (cnt * len_replace) + (cnt * len_replace_with) + 1;
    MALLOC(ret, newlen, return NULL);

    tmp = last = st_;
    writing = ret;
    for (int i=0; i < cnt; i++) {
        /* write the good piece */
        tmp = strstr(last, replace);
        memcpy(writing, last, tmp - last);
        /* update where we're writing and where we just matched */
        writing += tmp - last;
        last = tmp + len_replace;
        /* write the replacement string */
        memcpy(writing, replace_with, len_replace_with);
        /* update where we're writing */
        writing += len_replace_with;
    }
    if (writing < ret + newlen) {
        /* finish it out */
        memcpy(writing, last, st_ + len_st - last);
    }
    ret[newlen-1] = '\0';

finishup:
    FREE(replace_);
    FREE(replace_with_);
    FREE(st_);
    return ret;
} /* }}} */


char *strtrim(char *str) { /* {{{ */
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
} /* }}} */

void *task_download(CURL *curl, void *arg) { /* {{{ */
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
    cwr_fprintf(stderr, LOG_BRIEF, BRIEF_WARN "\t%s\t", (const char*)arg);
    cwr_fprintf(stderr, LOG_WARN, "%s%s%s is available in %s%s%s\n",
        colstr->pkg, (const char*)arg, colstr->nc,
        colstr->repo, db, colstr->nc);
    return NULL;
  }

  queryresult = task_query(curl, arg);
  if (!queryresult) {
    cwr_fprintf(stderr, LOG_BRIEF, BRIEF_ERR "\t%s\t", (const char*)arg);
    cwr_fprintf(stderr, LOG_ERROR, "no results found for %s\n", (const char*)arg);
    return NULL;
  }

  if (stat(arg, &st) == 0 && !cfg.force) {
    cwr_fprintf(stderr, LOG_BRIEF, BRIEF_ERR "\t%s\t", (const char*)arg);
    cwr_fprintf(stderr, LOG_ERROR, "`%s/%s' already exists. Use -f to overwrite.\n",
        cfg.dlpath, (const char*)arg);
    alpm_list_free_inner(queryresult, aurpkg_free);
    alpm_list_free(queryresult);
    return NULL;
  }

  response.data = NULL;
  response.size = 0;

  curl_easy_setopt(curl, CURLOPT_ENCODING, "identity"); /* disable compression */
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_response);

  escaped = curl_easy_escape(curl, arg, strlen(arg));
  cwr_asprintf(&url, AUR_PKG_URL, cfg.proto, escaped, escaped);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_free(escaped);

  curlstat = curl_easy_perform(curl);

  if (curlstat != CURLE_OK) {
    cwr_fprintf(stderr, LOG_BRIEF, BRIEF_ERR "\t%s\t", (const char*)arg);
    cwr_fprintf(stderr, LOG_ERROR, "[%s]: %s\n", (const char*)arg, curl_easy_strerror(curlstat));
    goto finish;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);

  switch (httpcode) {
    case 200:
      break;
    default:
      cwr_fprintf(stderr, LOG_BRIEF, BRIEF_ERR "\t%s\t", (const char*)arg);
      cwr_fprintf(stderr, LOG_ERROR, "[%s]: server responded with http%ld\n",
          (const char*)arg, httpcode);
      goto finish;
  }
  cwr_printf(LOG_BRIEF, BRIEF_OK "\t%s\t", (const char*)arg);
  cwr_printf(LOG_INFO, "%s%s%s downloaded to %s\n",
      colstr->pkg, (const char*)arg, colstr->nc, cfg.dlpath);

  ret = archive_extract_file(&response);
  if (ret != ARCHIVE_EOF && ret != ARCHIVE_OK) {
    cwr_fprintf(stderr, LOG_BRIEF, BRIEF_ERR "\t%s\t", (const char*)arg);
    cwr_fprintf(stderr, LOG_ERROR, "[%s]: failed to extract tarball\n",
        (const char*)arg);
    goto finish;
  }

  if (cfg.getdeps) {
    resolve_dependencies(curl, arg);
  }

finish:
  FREE(url);
  FREE(response.data);

  return queryresult;
} /* }}} */

void *task_query(CURL *curl, void *arg) { /* {{{ */
  alpm_list_t *pkglist = NULL;
  CURLcode curlstat;
  struct yajl_handle_t *yajl_hand = NULL;
  const char *argstr;
  char *escaped, *url;
  long httpcode;
  int span = 0;
  struct yajl_parser_t *parse_struct;

  /* find a valid chunk of search string */
  if (cfg.opmask & OP_SEARCH) {
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
  yajl_hand = yajl_alloc(&callbacks, NULL, (void*)parse_struct);

  curl = curl_init_easy_handle(curl);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, yajl_parse_stream);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, yajl_hand);

  escaped = curl_easy_escape(curl, argstr, span);
  if (cfg.opmask & OP_SEARCH) {
    cwr_asprintf(&url, AUR_RPC_URL, cfg.proto, AUR_QUERY_TYPE_SEARCH, escaped);
  } else if (cfg.opmask & OP_MSEARCH) {
    cwr_asprintf(&url, AUR_RPC_URL, cfg.proto, AUR_QUERY_TYPE_MSRCH, escaped);
  } else {
    cwr_asprintf(&url, AUR_RPC_URL, cfg.proto, AUR_QUERY_TYPE_INFO, escaped);
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);

  cwr_printf(LOG_DEBUG, "[%p]: curl_easy_perform %s\n", (void*)pthread_self(), url);
  curlstat = curl_easy_perform(curl);

  if (curlstat != CURLE_OK) {
    cwr_fprintf(stderr, LOG_ERROR, "[%s]: %s\n", (const char*)arg,
        curl_easy_strerror(curlstat));
    goto finish;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode >= 300) {
    cwr_fprintf(stderr, LOG_ERROR, "[%s]: server responded with http%ld\n",
        (const char*)arg, httpcode);
    goto finish;
  }

  yajl_complete_parse(yajl_hand);

  pkglist = parse_struct->pkglist;

  if (pkglist && cfg.extinfo) {
    struct aurpkg_t *aurpkg;
    char *pburl, *pkgbuild, *aurpkgurl, *aurpkgpage;

    aurpkg = alpm_list_getdata(pkglist);

    cwr_asprintf(&pburl, AUR_PKGBUILD_PATH, cfg.proto, escaped);
    pkgbuild = curl_get_url_as_buffer(curl, pburl);
    free(pburl);

    alpm_list_t **pkg_details[PKGDETAIL_MAX] = {
      &aurpkg->depends, &aurpkg->makedepends, &aurpkg->optdepends,
      &aurpkg->provides, &aurpkg->conflicts, &aurpkg->replaces
    };

    pkgbuild_get_extinfo(pkgbuild, pkg_details);
    free(pkgbuild);

    cwr_asprintf(&aurpkgurl, AUR_PKG_URL_FORMAT "%s", cfg.proto, aurpkg->id);
    aurpkgpage = curl_get_url_as_buffer(curl, aurpkgurl);
    aurpkg->comments = get_aur_comments(aurpkgpage);
  }

finish:
  yajl_free(yajl_hand);
  curl_free(escaped);
  FREE(parse_struct);
  FREE(url);

  return pkglist;
} /* }}} */

void *task_update(CURL *curl, void *arg) { /* {{{ */
  pmpkg_t *pmpkg;
  struct aurpkg_t *aurpkg;
  void *dlretval, *qretval;

  if (alpm_list_find_str(cfg.ignore.pkgs, arg)) {
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
      if (cfg.opmask & OP_DOWNLOAD) {
        /* we don't care about the return, but we do care about leaks */
        dlretval = task_download(curl, (void*)aurpkg->name);
        alpm_list_free_inner(dlretval, aurpkg_free);
        alpm_list_free(dlretval);
      } else {
        if (cfg.quiet) {
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
} /* }}} */

void *thread_pool(void *arg) { /* {{{ */
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
    /* try to pop off the work queue */
    pthread_mutex_lock(&lock);
    job = alpm_list_getdata(workq);
    workq = alpm_list_next(workq);
    pthread_mutex_unlock(&lock);

    /* make sure we hooked a new job */
    if (!job) {
      break;
    }

    ret = alpm_list_join(ret, task->threadfn(curl, job));
  }

  curl_easy_cleanup(curl);

  return ret;
} /* }}} */

void usage() { /* {{{ */
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
      "      --ignorerepo <repo> ignore some or all binary repos\n"
      "      --nossl             do not use https connections\n"
      "  -n, --comments          print comments from the AUR web interface (implies -ii)\n"
      "  -t, --target <dir>      specify an alternate download directory\n"
      "      --threads <num>     limit number of threads created\n"
      "      --timeout <num>     specify connection timeout in seconds\n"
      "  -V, --version           display version\n\n");
  fprintf(stderr, " Output options:\n"
      "  -b, --brief             show output in a more script friendly format\n"
      "  -c, --color[=WHEN]      use colored output. WHEN is `never', `always', or `auto'\n"
      "      --debug             show debug output\n"
      "      --format <string>   print package output according to format string\n"
      "      --listdelim <delim> change list format delimeter\n"
      "  -q, --quiet             output less\n"
      "  -v, --verbose           output more\n\n");
} /* }}} */

void version() { /* {{{ */
  printf("\n  " COWER_VERSION "\n");
  printf("     \\\n"
         "      \\\n"
         "        ,__, |    |\n"
         "        (oo)\\|    |___\n"
         "        (__)\\|    |   )\\_\n"
         "          U  |    |_w |  \\\n"
         "             |    |  ||   *\n"
         "\n"
         "             Cower....\n\n");
} /* }}} */

size_t yajl_parse_stream(void *ptr, size_t size, size_t nmemb, void *stream) { /* {{{ */
  struct yajl_handle_t *hand;
  size_t realsize = size * nmemb;

  hand = (struct yajl_handle_t*)stream;
  yajl_parse(hand, ptr, realsize);

  return realsize;
} /* }}} */

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

  /* initialize config */
  memset(&cfg, 0, sizeof cfg);
  cfg.color = cfg.maxthreads = cfg.timeout = UNSET;
  cfg.delim = LIST_DELIM;
  cfg.logmask = LOG_ERROR|LOG_WARN|LOG_INFO; 
  cfg.proto = "https";

  ret = parse_options(argc, argv);
  switch (ret) {
    case 0: /* everything's cool */
      break;
    case 3:
      fprintf(stderr, "error: no operation specified (use -h for help)\n");
    case 1: /* these provide their own error mesg */
    case 2:
      return ret;
  }

  if ((ret = parse_configfile() != 0)) {
    return ret;
  }

  /* fallback from sentinel values */
  cfg.maxthreads = cfg.maxthreads == UNSET ? THREAD_DEFAULT : cfg.maxthreads;
  cfg.timeout = cfg.timeout == UNSET ? TIMEOUT_DEFAULT : cfg.timeout;
  cfg.color = cfg.color == UNSET ? 0 : cfg.color;

  if ((ret = strings_init()) != 0) {
    return ret;
  }

  if ((ret = set_working_dir()) != 0) {
    goto finish;
  }

  cwr_printf(LOG_DEBUG, "initializing curl\n");
  if (STREQ(cfg.proto, "https")) {
    ret = curl_global_init(CURL_GLOBAL_SSL);
    openssl_crypto_init();
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
  if ((cfg.opmask & OP_UPDATE) && !cfg.targets) {
    cfg.targets = alpm_find_foreign_pkgs();
  }

  workq = cfg.targets;
  num_threads = alpm_list_count(cfg.targets);
  if (num_threads == 0) {
    fprintf(stderr, "error: no targets specified (use -h for help)\n");
    goto finish;
  } else if (num_threads > cfg.maxthreads) {
    num_threads = cfg.maxthreads;
  }

  CALLOC(threads, num_threads, sizeof *threads, goto finish);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  /* override task behavior */
  if (cfg.opmask & OP_UPDATE) {
    task.threadfn = task_update;
  } else if (cfg.opmask & OP_INFO) {
    task.printfn = cfg.format ? print_pkg_formatted : print_pkg_info;
  } else if (cfg.opmask & (OP_SEARCH|OP_MSEARCH)) {
    task.printfn = cfg.format ? print_pkg_formatted : print_pkg_search;
  } else if (cfg.opmask & OP_DOWNLOAD) {
    task.threadfn = task_download;
  }

  /* filthy, filthy hack: prepopulate the package cache */
  alpm_db_get_pkgcache(db_local);

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
    results = alpm_list_join(results, thread_return);
  }

  free(threads);
  pthread_attr_destroy(&attr);

  /* we need to exit with a non-zero value when:
   * a) search/info/download returns nothing
   * b) update (without download) returns something
   * this is opposing behavior, so just XOR the result on a pure update */
  results = filter_results(results);
  ret = ((results == NULL) ^ !(cfg.opmask & ~OP_UPDATE));
  print_results(results, task.printfn);
  alpm_list_free_inner(results, aurpkg_free);
  alpm_list_free(results);

  openssl_crypto_cleanup();

finish:
  FREE(cfg.dlpath);
  FREELIST(cfg.targets);
  FREELIST(cfg.ignore.pkgs);
  FREELIST(cfg.ignore.repos);
  FREE(colstr);

  cwr_printf(LOG_DEBUG, "releasing curl\n");
  curl_global_cleanup();

  cwr_printf(LOG_DEBUG, "releasing alpm\n");
  alpm_release();

  return ret;
}

/* vim: set et sw=2: */
/* Local Variables: */
/* c-basic-offset:2 */
/* indent-tabs-mode:nil */
/* End: */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "alpmutil.h"
#include "conf.h"
#include "depends.h"
#include "download.h"
#include "search.h"
#include "util.h"

void alpm_free_str(void *data) {
  FREE(data);
}

alpm_list_t *parse_bash_array(alpm_list_t *deplist, char *startdep) {

  char *token;

  while ((token = strsep(&startdep, " ")) != NULL) {
    if (strlen(token) <= 1) continue; /* Kludgy */
    if (*token == '\'' || *token == '\"')
      token++;
    char *i;
    for (i = token; *i != '\0'; i++) {
      switch (*i) {
      case '=':
      case '<':
      case '>':
      case '\'':
        *i = '\0';
        break;
      }
    }
    deplist = alpm_list_add(deplist, strdup(token));
  }

  return deplist;

}

alpm_list_t *parsedeps(alpm_list_t *deplist) {
  FILE* fd;
  char *deps, *tmplist;
  char buffer[BUFSIZ];

  fd = fopen("PKGBUILD", "r");
  fread(buffer, sizeof(char), BUFSIZ, fd); 

  /* depends */
  deps = strstr(buffer, "depends=(");
  if (deps) {
    tmplist = strndup(deps + 9, strchr(deps, ')') - deps);
    deplist = parse_bash_array(deplist, tmplist);
    free(tmplist);
  }

  /* makedepends */
  deps = strstr(buffer, "makedepends=(");
  if (deps) {
    tmplist = strndup(deps + 13, strchr(deps, ')') - deps);
    deplist = parse_bash_array(deplist, tmplist);
    free(tmplist);
  }

  fclose(fd);

  return deplist;
}

int get_pkg_dependencies(const char *pkgbuild_path) {
  extern pmdb_t *db_local;

  struct stat st;
  if (stat(pkgbuild_path, &st)) {
    fprintf(stderr, "Error: unable to find PKGBUILD.\n");
    return -1;
  }

  alpm_list_t *deplist = NULL;
  deplist = parsedeps(deplist);

  if (! config->quiet) {
    if (config->color) {
      cprintf("Attempting to fetch uninstalled %<dependencies%>...\n", YELLOW);
    } else {
      printf("Attempting to fetch uninstalled dependencies...\n");
    }
  }

  alpm_list_t *i;
  for (i = deplist; i; i = alpm_list_next(i)) {
    const char* depend = i->data;
    if (alpm_db_get_pkg(db_local, depend)) { /* installed */
      continue;
    }

    if (is_in_pacman(depend)) { /* available in pacman */
      continue;
    }

    /* if we're here, we need to check the AUR */
    json_t *infojson = aur_rpc_query(AUR_QUERY_TYPE_INFO, depend);
    if (infojson) {
      aur_get_tarball(infojson);
    }

  }

  /* Cleanup */
  alpm_list_free_inner(deplist, (alpm_list_fn_free)alpm_free_str);
  alpm_list_free(deplist);

  return 0;
}


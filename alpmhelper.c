#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include <alpm.h>

#include "util.h"
#include "alpmhelper.h"

static pmdb_t *db_local;

void alpm_quick_init() {
    /* Should be parsing config here. For now, static declares */
    alpm_initialize();
    alpm_option_set_root("/");
    alpm_option_set_dbpath("/var/lib/pacman/");
    alpm_db_register_sync("core");
    alpm_db_register_sync("extra");
    alpm_db_register_sync("testing");
    alpm_db_register_sync("community");
    alpm_db_register_sync("community-testing");
    db_local = alpm_db_register_local();
}

static int is_foreign(pmpkg_t *pkg) {
    const char *pkgname = alpm_pkg_get_name(pkg);
    alpm_list_t *j;
    alpm_list_t *sync_dbs = alpm_option_get_syncdbs();

    int match = 0;
    for(j = sync_dbs; j; j = alpm_list_next(j)) {
        pmdb_t *db = alpm_list_getdata(j);
        pmpkg_t *findpkg = alpm_db_get_pkg(db, pkgname);
        if(findpkg) {
            match = 1;
            break;
        }
    }
    if(match == 0) {
        return 1 ;
    }
    return 0;
}

/* Equivalent of pacman -Qs or -Qm */
alpm_list_t *alpm_query_search(alpm_list_t *targets) {
    alpm_list_t *i, *searchlist;
    int freelist;

    alpm_list_t *ret = NULL;

    /* if we have a targets list, search for packages matching it */
    if(targets) {
        searchlist = alpm_db_search(db_local, targets);
        freelist = 1;
    } else { /* Otherwise, we want a list of foreign packages */
        searchlist = alpm_db_get_pkgcache(db_local);
        freelist = 0;
    }

    if(searchlist == NULL) {
        return NULL;
    }

    for(i = searchlist; i; i = alpm_list_next(i)) {
        pmpkg_t *pkg = alpm_list_getdata(i);

        /* only weed out foreign pkgs if we called this
         * function with a NULL parameter
         */
        if (!targets && is_foreign(pkg)) { 
            ret = alpm_list_add(ret, pkg);
        }
    }

    /* we only want to free if the list was a search list */
    if(freelist) {
        alpm_list_free(searchlist);
    }

    return ret; /* This needs to be freed in the calling function */
}


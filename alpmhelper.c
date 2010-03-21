/*
 *  alpmhelper.c
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
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Non-standard */
#include <alpm.h>
#include <jansson.h>

/* Local */
#include "alpmhelper.h"
#include "util.h"

extern int opt_mask;
static pmdb_t *db_local;

void alpm_quick_init() {

    /* TODO: Parse pacman config */
    alpm_initialize();
    alpm_option_set_root("/");
    alpm_option_set_dbpath("/var/lib/pacman/");
    alpm_db_register_sync("testing");
    alpm_db_register_sync("community-testing");
    alpm_db_register_sync("core");
    alpm_db_register_sync("extra");
    alpm_db_register_sync("community");
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
alpm_list_t *alpm_query_search(alpm_list_t *target) {

    alpm_list_t *i, *searchlist, *ret = NULL;
    int freelist;

    if (target) { /* If we have a target, search for it */
        searchlist = alpm_db_search(db_local, target);
        freelist = 1;
    } else { /* Otherwise return a pointer to the local cache */
        searchlist = alpm_db_get_pkgcache(db_local);
        freelist = 0;
    }

    if(searchlist == NULL) {
        return NULL;
    }

    for(i = searchlist; i; i = alpm_list_next(i)) {
        pmpkg_t *pkg = alpm_list_getdata(i);

        /* TODO: Reuse alpm_sync_search for this */
        if (!target && is_foreign(pkg)) {
            ret = alpm_list_add(ret, pkg);
        }
    }

    if (freelist) {
        alpm_list_free(searchlist);
    }

    return ret; /* This needs to be freed in the calling function */
}


/* Semi-equivalent to pacman -Si. Quits as soon as a pkg is found */
pmdb_t *alpm_sync_search(alpm_list_t *target) {

    pmdb_t *db = NULL;
    alpm_list_t *syncs, *i, *j;

    syncs = alpm_option_get_syncdbs();

    /* Iterating over each sync */
    for (i = syncs; i; i = alpm_list_next(i)) {
        db = alpm_list_getdata(i);

        /* Iterating over each package in sync */
        for (j = alpm_db_get_pkgcache(db); j; j = alpm_list_next(j)) {
            pmpkg_t *pkg = alpm_list_getdata(j);
            if (strcmp(alpm_pkg_get_name(pkg), alpm_list_getdata(target)) == 0) {
                return db;
            }
        }
    }

    return NULL; /* Not found */
}

int is_in_pacman(const char *target) {

    pmdb_t *found_in;
    alpm_list_t *p = NULL;

    p = alpm_list_add(p, (void*)target);
    found_in = alpm_sync_search(p);

    if (found_in) {
        opt_mask & OPT_COLOR ? cfprint(1, target, WHITE) :
            printf("%s", target);
        printf(" is available in "); 
        opt_mask & OPT_COLOR ? cfprint(1, alpm_db_get_name(found_in), YELLOW) :
            printf("%s", alpm_db_get_name(found_in));
        putchar('\n');

        alpm_list_free(p);
        return 1;
    }

    alpm_list_free(p);
    return 0;
}

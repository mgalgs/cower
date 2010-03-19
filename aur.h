#ifndef _AUR_H
#define _AUR_H

#define URL_FORMAT      "http://aur.archlinux.org/rpc.php?type=%s&arg=%s"
#define URL_SIZE        256

#define BUFFER_SIZE     (256 * 1024) /* 256kB */

/* Info */
struct aurpkg *aur_pkg_info(char*, int*);

/* Search */
void print_search_results(json_t*, int*);
struct json_t *aur_pkg_search(char*, int*);

/* Download */
int get_taurball(const char*, char*, int*);

#endif /* _AUR_H */

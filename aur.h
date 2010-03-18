#define URL_FORMAT      "http://aur.archlinux.org/rpc.php?type=%s&arg=%s"
#define URL_SIZE        256

#define BUFFER_SIZE     (256 * 1024) /* 256kB */

/* Search */
struct json_t *aur_pkg_search(char*);
void print_search_results(json_t*);

/* Info */
struct aurpkg *aur_pkg_info(char*);

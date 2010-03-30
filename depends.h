
#include <alpm.h>

int fetch_depends(alpm_list_t*);
int get_pkg_dependencies(const char*);
alpm_list_t *parse_bash_array(alpm_list_t*, char*);
alpm_list_t *parsedeps(alpm_list_t*);


/* Color constants. Add for bold */
#define BLACK   0
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define MAGENTA 5
#define CYAN    6
#define WHITE   7
#define FG      8
#define BOLD    9

/* Operations */
enum {
    OPER_SEARCH = 1,
    OPER_UPDATE = 2,
    OPER_INFO = 4,
    OPER_DOWNLOAD = 8,
};

/* Options */
enum {
    OPT_COLOR = 1,
    OPT_VERBOSE = 2,
    OPT_FORCE = 4,
};

char *colorize(const char*, int, char*);
int file_exists(const char*);

#include <stdio.h>
#include <string.h>

#include "util.h"

char *colorize(const char* input, int color, char* buffer) {
    sprintf(buffer, "\033[1;3%dm%s\033[1;m\0", color, input);
    return buffer;
}

int file_exists(const char* filename) {
    FILE *fd;
    if (fd = fopen(filename, "r")) {
        fclose(fd);
        return 1;
    } else {
        return 0;
    }
}

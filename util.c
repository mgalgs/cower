#include <stdio.h>
#include <string.h>

#include "util.h"

char *colorize(const char* input, int color, char* buffer) {
    sprintf(buffer, "\033[1;3%dm%s\033[1;m\0", color, input);
    return buffer;
}

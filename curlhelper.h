#ifndef _CURLHELPER_H
#define _CURLHELPER_H

struct write_result {
    char *data;
    int pos;
};

int newline_offset(const char *text);
size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream);
char *request(const char *url);

#endif /* _CURLHELPER_H */

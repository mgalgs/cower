struct write_result {
    char *data;
    int pos;
};

int newline_offset(const char *text);
size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream);
char *request(const char *url);

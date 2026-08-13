const char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return haystack; 
    for (; *haystack; ++haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) { ++h; ++n; }
        if (!*n) return haystack;
    }
    return 0; 
}

int str_starts_with(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}
int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}
int str_eq_n(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
        if (a[i] == '\0') return 1;
    }
    return 1;
}
int str_len(const char* s) { int n = 0; while (s[n]) n++; return n; }

void str_copy(char* dst, const char* src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}
int is_space_c(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
int is_alpha_c(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-'; }

void skip_spaces(const char** p) {
    while (is_space_c(**p)) (*p)++;
}

int is_digit_c(char c) { return c >= '0' && c <= '9'; }

int hex_digit_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int parse_int(const char** p) {
    const char* s = *p;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    if (!is_digit_c(*s)) return 0;
    int val = 0;
    while (is_digit_c(*s)) { val = val * 10 + (*s - '0'); s++; }
    *p = s;
    return val * sign;
}
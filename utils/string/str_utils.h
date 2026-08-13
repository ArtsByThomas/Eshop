#ifndef STRING_UTILS_H
#define STRING_UTILS_H



// --- Vyhledávání a porovnávání řetězců ---
const char* strstr(const char* haystack, const char* needle);
int str_starts_with(const char* str, const char* prefix);
int str_eq(const char* a, const char* b);
int str_eq_n(const char* a, const char* b, int n);

// --- Délka a kopírování ---
int str_len(const char* s);
void str_copy(char* dst, const char* src, int max_len);

// --- Práce se znaky ---
int is_space_c(char c);
int is_alpha_c(char c);
int is_digit_c(char c);
int hex_digit_val(char c);

// --- Parsování ---
void skip_spaces(const char** p);
int parse_int(const char** p);



#endif /* STRING_UTILS_H */
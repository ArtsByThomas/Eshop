#ifndef NAVIGATION_H
#define NAVIGATION_H



// --- Makra ---
#define MAX_HISTORY 64
#define HOME_URL "http://192.168.2.1:8000/"

// --- Struktury ---
typedef struct {
    int body_len;
    char *resp_buf;
    int resp_max;
} HttpPostArgs;

typedef struct {
    const char *body;
    int body_len;
    char *resp_buf;
    int resp_max;
} HttpsPostArgs;


// --- Globální proměnné (deklarace pomocí extern) ---
extern uint32_t current_server_ip;
extern char current_domain[128];
extern char current_path[128];
extern int current_is_https; 
extern int current_port;      

extern char address_bar_text[128];
extern int address_bar_len;
extern int address_bar_focused;

extern char nav_history[MAX_HISTORY][128];
extern int nav_history_count;
extern int nav_history_pos;    
extern int nav_history_navigating; // 1 = právě navigujeme back/forward (nepřidávat znovu)

extern int bytes_global;

// --- Síťové volání (Syscalls a HTTP/HTTPS) ---
// Plain HTTP (bez TLS)
int sys_http_get(uint32_t ip, uint16_t port, const char *path, char *buf, int max_len);
int sys_https_get(uint32_t ip, const char *domain, uint16_t port, const char *path, char *buf, int max_len);

// HTTP/HTTPS POST s tělem requestu
int sys_http_post(uint32_t ip, uint16_t port, const char *path, const char *body, int body_len, char *buf, int max_len);
int sys_https_post(uint32_t ip, const char *domain, uint16_t port, const char *path, const char *body, int body_len, char *buf, int max_len);

// Práce s cookies
void sys_cookie_get(const char *domain, int is_https, char *out, int out_size);
void sys_cookie_set(const char *domain, const char *cookie_str);


// --- Navigace a Historie ---
void resolve_relative_url(const char* href, char* out, int out_size);
void navigate_to(const char* url);

void nav_history_push(const char* url);
void nav_go_back(void);
void nav_go_forward(void);
void nav_reload(void);
void nav_go_home(void);

// Sdílená "po stažení" logika pro navigate_to (GET) i navigate_to_post
void process_navigation_response(const char* full_url, const char* path, int bytes);


// --- Pomocné funkce (Utils) ---
void append_num(int num, char** buf);
void ip_to_string(uint32_t ip, char* buffer);
void debug_print_int(int num);

#endif // NAVIGATION_H
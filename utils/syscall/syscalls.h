#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h> // Nutné pro uint32_t a uint16_t



// --- Základní systémová volání (wrappery) ---
long syscall0(long nr);
long syscall3(long nr, long arg1, long arg2, long arg3);
long syscall5(long nr, long arg1, long arg2, long arg3, long arg4, long arg5);
long syscall6(long nr, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

// --- Základní OS funkce ---
int sys_read(int fd, char *buf, int len);
void sys_yield(void);
void sys_exit(void);
long sys_get_time_ms(void);

// --- Grafika a vstupy (UI) ---
int sys_fb_blit(const uint32_t *pixel_buf, int w, int h);
int sys_get_mouse(int *x, int *y, int *buttons);
int sys_get_window_size(int *w, int *h);

// --- Sítě a Web ---
uint32_t sys_dns_resolve(const char *domain);
int sys_https_get(uint32_t ip, const char *domain, uint16_t port, const char *path, char *buf, int max_len);
int sys_http_get(uint32_t ip, uint16_t port, const char *path, char *buf, int max_len);
void sys_cookie_get(const char *domain, int is_https, char *out, int out_size);
void sys_cookie_set(const char *domain, const char *cookie_str);

// --- Ostatní / Různé ---
long vk_call(int func_id, long a1, long a2);
void debug_print(const char* text);



#endif /* SYSCALLS_H */
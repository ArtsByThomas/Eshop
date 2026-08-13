#include <stdlib.h>
int str_len(const char* s);
long syscall0(long nr) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr) : "rcx", "r11", "memory");
    return ret;
}

long syscall3(long nr, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(arg1), "S"(arg2), "d"(arg3) : "rcx", "r11", "memory");
    return ret;
}

long syscall5(long nr, long arg1, long arg2, long arg3, long arg4, long arg5) {
    long ret;
    register long r10 __asm__("r10") = arg4;
    register long r8  __asm__("r8")  = arg5;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
    return ret;
}
long syscall6(long nr, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6) {
    long ret;
    register long r10 __asm__("r10") = arg4;
    register long r8  __asm__("r8")  = arg5;
    register long r9  __asm__("r9")  = arg6; 
    
    __asm__ volatile(
        "syscall" 
        : "=a"(ret) 
        : "a"(nr), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9) 
        : "rcx", "r11", "memory"
    );
    
    return ret;
}
int sys_read(int fd, char *buf, int len) { return syscall3(0, fd, (long)buf, len); }
void sys_yield() { syscall0(24); } 
void sys_exit()  { syscall0(60); } 
int sys_fb_blit(const uint32_t *pixel_buf, int w, int h) { return syscall3(1010, (long)pixel_buf, (long)w, (long)h); }


int sys_get_mouse(int *x, int *y, int *buttons) {
    return syscall3(1012, (long)x, (long)y, (long)buttons);
}

int sys_get_window_size(int *w, int *h) {
    return syscall3(1014, (long)w, (long)h, 0);
}
uint32_t sys_dns_resolve(const char *domain) {
    return syscall3(1013, (long)domain, 0, 0); 
}
long sys_get_time_ms(void) {
    long tv[2] = {0, 0}; 
    syscall3(78, (long)tv, 0, 0);
    return tv[0] * 1000L + tv[1] / 1000L;
}
long vk_call(int func_id, long a1, long a2) {
    return syscall3(1030, (long)func_id, a1, a2);
}
void debug_print(const char* text) {
    syscall3(100, (long)text, (long)str_len(text), 0);
}

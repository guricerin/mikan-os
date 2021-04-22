#include <errno.h>
#include <sys/types.h>

void _exit(void) {
    while (1) __asm__("hlt");
}

/// program breakとその上限
caddr_t g_program_break, g_program_break_end;

/// program break を増減
/// program break : 各プロセスが使えるメモリ領域の末尾アドレス
/// これを後ろにずらすことで新たなメモリ領域を確保できる
caddr_t sbrk(int incr) {
    if (g_program_break == 0 || g_program_break_end <= g_program_break + incr) {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    caddr_t prev_break = g_program_break;
    g_program_break += incr;
    return prev_break;
}

int getpid(void) {
    return 1;
}

int kill(int pid, int sig) {
    errno = EINVAL;
    return -1;
}

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "syscall.h"

int close(int fd) {
    errno = EBADF;
    return -1;
}

int fstat(int fd, struct stat* buf) {
    errno = EBADF;
    return -1;
}

pid_t getpid(void) {
    return 0;
}

int isatty(int fd) {
    errno = EBADF;
    return -1;
}

int kill(pid_t pid, int sig) {
    errno = EPERM;
    return -1;
}

off_t lseek(int fd, off_t offset, int whence) {
    errno = EBADF;
    return -1;
}

int open(const char* path, int flags) {
    struct SyscallResult res = SyscallOpenFile(path, flags);
    if (res.error == 0) {
        return res.value;
    }
    errno = res.error;
    return -1;
}

ssize_t read(int fd, void* buf, size_t count) {
    struct SyscallResult res = SyscallReadFile(fd, buf, count);
    if (res.error == 0) {
        return res.value;
    }
    errno = res.error;
    return -1;
}

/// メモリ非割り当て領域の先頭アドレスをいじくる
/// -> 加算ならメモリ確保、減算ならメモリ解放
caddr_t sbrk(int incr) {
    static uint8_t heap[4096];
    static int i = 0;
    int prev = i;
    i += incr;
    return (caddr_t)&heap[prev];
}

ssize_t write(int fd, const void* buf, size_t count) {
    struct SyscallResult res = SyscallPutString(fd, (uint64_t)buf, count);
    if (res.error == 0) {
        return res.value;
    }
    errno = res.error;
    return -1;
}

void _exit(int status) {
    SyscallExit(status);
}
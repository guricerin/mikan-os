#include <cstddef>
#include <cstdint>

#include "../kernel/logger.hpp"

extern "C" {

struct SyscallResult {
    uint64_t value;
    int error;
};

SyscallResult SyscallLogString(LogLevel level, const char* message);
SyscallResult SyscallPutString(uint64_t, uint64_t, uint64_t);
void SyscallExit(int exit_code);
SyscallResult SyscallOpenWindow(int w, int h, int x, int y, const char* title);
}
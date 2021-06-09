#include "../kernel/logger.hpp"
#include <cstdint>

extern "C" {

struct SyscallResult {
    uint64_t value;
    int error;
};

int64_t SyscallLogString(LogLevel, const char*);
struct SyscallResult SyscallPutString(uint64_t, uint64_t, uint64_t);
void SyscallExit(int exit_code);
}
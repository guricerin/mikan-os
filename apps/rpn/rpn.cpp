#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../kernel/logger.hpp"

int g_stack_ptr;
long g_stack[100];

long Pop() {
    long value = g_stack[g_stack_ptr];
    g_stack_ptr--;
    return value;
}

void Push(long value) {
    g_stack_ptr++;
    g_stack[g_stack_ptr] = value;
}

extern "C" int64_t SyscallLogString(LogLevel, const char*);

extern "C" void SyscallExit(int exit_code);

extern "C" void main(int argc, char** argv) {
    g_stack_ptr = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "+") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a + b);
        } else if (strcmp(argv[i], "-") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a - b);
        } else {
            long a = atol(argv[i]);
            Push(a);
        }
    }

    long result = 0;
    if (g_stack_ptr >= 0) {
        result = Pop();
    }

    printf("%ld\n", result);
    SyscallExit(static_cast<int>(result));
}
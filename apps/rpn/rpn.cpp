#include <cstdint>
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

extern "C" int main(int argc, char** argv) {
    g_stack_ptr = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "+") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a + b);
            SyscallLogString(kWarn, "+");
        } else if (strcmp(argv[i], "-") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a - b);
            SyscallLogString(kWarn, "-");
        } else {
            long a = atol(argv[i]);
            Push(a);
            SyscallLogString(kWarn, "#");
        }
    }

    if (g_stack_ptr < 0) {
        return 0;
    }
    SyscallLogString(kWarn, "\nhello, this is rpn\n");
    while (1)
        ;
    return static_cast<int>(Pop());
}
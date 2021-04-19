#include "logger.hpp"

#include <cstddef>
#include <cstdio>

#include "console.hpp"

namespace {
    LogLevel g_log_level = LogLevel::Warn;
}

extern Console* g_console;

void SetLogLevel(LogLevel level) {
    g_log_level = level;
}

int Log(LogLevel level, const char* format, ...) {
    if (level > g_log_level) {
        return 0;
    }

    va_list ap;
    va_start(ap, format);
    char s[1024];
    int result = vsprintf(s, format, ap);
    va_end(ap);

    g_console->PutString(s);
    return result;
}
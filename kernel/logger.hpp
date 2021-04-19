#pragma once

enum class LogLevel {
    Error = 3,
    Warn = 4,
    Info = 6,
    Debug = 7,
};

/// グローバルなログ優先度のしきい値をlevelに設定する．
/// 以降のLogの呼び出しでは，ここで設定した優先度以上のログのみ記録される．
void SetLogLevel(LogLevel level);

int Log(LogLevel level, const char* format, ...);
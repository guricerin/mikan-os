#pragma once

enum LogLevel {
    kError = 3,
    kWarn = 4,
    kInfo = 6,
    kDebug = 7,
};

/// グローバルなログ優先度のしきい値をlevelに設定する．
/// 以降のLogの呼び出しでは，ここで設定した優先度以上のログのみ記録される．
void SetLogLevel(LogLevel level);

int Log(LogLevel level, const char* format, ...);
#pragma once

#include <cstddef>

#include "error.hpp"

/// 文字列（orバイト列）を扱える何か
class IFileDescriptor {
public:
    virtual ~IFileDescriptor() = default;
    virtual size_t Read(void* buf, size_t len) = 0;
    virtual size_t Write(const void* buf, size_t len) = 0;
    virtual size_t Size() const = 0;

    /// Load() reads file content without changing internal offset
    virtual size_t Load(void* buf, size_t len, size_t offset) = 0;
};

/// 指定ファイルディスクリプタに文字列を書き込む
size_t PrintToFD(IFileDescriptor& fd, const char* format, ...);
/// 指定文字に出会うまで1byteずつ読み取る
size_t ReadDelim(IFileDescriptor& fd, char delim, char* buf, size_t len);

#pragma once

/// 文字列（orバイト列）を扱える何か
class IFileDescriptor {
public:
    virtual ~IFileDescriptor() = default;
    virtual size_t Read(void* buf, size_t len) = 0;
    virtual size_t Write(const void* buf, size_t len) = 0;
};

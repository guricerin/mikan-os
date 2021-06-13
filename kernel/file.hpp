#pragma once

/// 文字列（orバイト列）を読み取れる何か
class IFileDescriptor {
public:
    virtual ~IFileDescriptor() = default;
    virtual size_t Read(void* buf, size_t len) = 0;
};

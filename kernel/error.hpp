#pragma once

#include <array>
#include <cstdio>

enum class ErrorCode {
    Success,
    Full,
    Empty,
    NoEnoughMemory,
    IndexOutOfRange,
    HostContorollerNotHalted,
    InvalidSlotID,
    PortNotConnected,
    InvalidEndpointNumber,
    TransferRingNotSet,
    AlreadyAllocated,
    NotImplemented,
    InvalidDescriptor,
    BufferTooSmall,
    UnknownDevice,
    NoCorrespondingSetupStage,
    TransferFailed,
    InvalidPhase,
    UnknownXHCISpeedID,
    NoWaiter,
    LastOfCode, // この列挙子は常に最後に記述
};

class Error {
private:
    ErrorCode _code;
    const char* _file;
    int _line;

public:
    Error(ErrorCode code, const char* file, int line)
        : _code{code}, _file{file}, _line{line} {}

    operator bool() const {
        return this->_code != ErrorCode::Success;
    }

    const char* Name() const {
        return _code_names[static_cast<int>(this->_code)];
    }

    ErrorCode Cause() const {
        return this->_code;
    }

    const char* File() const {
        return this->_file;
    }

    int Line() const {
        return this->_line;
    }

private:
    static constexpr std::array _code_names = {
        "Success",
        "Full",
        "Empty",
        "NoEnoughMemory",
        "IndexOutOfRange",
        "HostControllerNotHalted",
        "InvalidSlotID",
        "PortNotConnected",
        "InvalidEndpointNumber",
        "TransferRingNotSet",
        "AlreadyAllocated",
        "NotImplemented",
        "InvalidDescriptor",
        "BufferTooSmall",
        "UnknownDevice",
        "NoCorrespondingSetupStage",
        "TransferFailed",
        "InvalidPhase",
        "UnknownXHCISpeedID",
        "NoWaiter",
    };
    static_assert(static_cast<size_t>(ErrorCode::LastOfCode) == _code_names.size());
};

#define MAKE_ERROR(code) Error((code), __FILE__, __LINE__)

template <class T>
struct WithError {
    T value;
    Error error;
};
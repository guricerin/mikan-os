#pragma once

#include <array>

enum class ErrorCode {
    Success,
    Full,
    Empty,
    Lastofcode,
};

class Error {
public:
    Error(ErrorCode code) : _code{code} {}

    operator bool() const {
        return this->_code != ErrorCode::Success;
    }

    const char* name() const {
        return _code_names[static_cast<int>(this->_code)];
    }

private:
    static constexpr std::array<const char*, 3> _code_names = {
        "success",
        "full",
        "empty",
    };

    ErrorCode _code;
};

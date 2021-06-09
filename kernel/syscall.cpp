#include "syscall.hpp"

#include <array>
#include <cstdint>

#include "asmfunc.h"
#include "logger.hpp"
#include "msr.hpp"

namespace syscall {

/// システムコールはこのマクロを使って実装する
#define SYSCALL(name)                                \
    int64_t name(                                    \
        uint64_t arg1, uint64_t arg2, uint64_t arg3, \
        uint64_t arg4, uint64_t arg5, uint64_t arg6)

    SYSCALL(LogString) {
        if (arg1 != kError && arg1 != kWarn && arg1 != kInfo && arg1 != kDebug) {
            return -1;
        }
        const char* s = reinterpret_cast<const char*>(arg2);
        if (strlen(s) > 1024) {
            return -1;
        }
        Log(static_cast<LogLevel>(arg1), "%s", s);
        return 0;
    }
#undef SYSCALL

} // namespace syscall

using SyscallFuncType = int64_t(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/// システムコールの（関数ポインタ）テーブル
/// この添字に0x80000000を足した値をシステムコール番号とする
extern "C" std::array<SyscallFuncType*, 1> syscall_table{
    /* 0x00 */ syscall::LogString,
};

void InitializeSyscall() {
    // syscallを使用可能にする
    WriteMSR(kIA32_EFER, 0x0501u);
    // syscallが実行されたときにコールされる関数を登録
    WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry));
    // IA32_STARレジスタに登録された値は、syscall / sysretを実行した際にCSレジスタ / SSレジスタに書き込まれる
    WriteMSR(kIA32_STAR, static_cast<uint64_t>(8) << 32 | static_cast<uint64_t>(16 | 3) << 48);
    WriteMSR(kIA32_FMASK, 0);
}
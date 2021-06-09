#include "syscall.hpp"

#include <array>
#include <cerrno>
#include <cstdint>

#include "asmfunc.h"
#include "logger.hpp"
#include "msr.hpp"
#include "task.hpp"
#include "terminal.hpp"

namespace syscall {
    /// システムコールの戻り値型
    struct Result {
        uint64_t value;
        int error;
    };

/// システムコールはこのマクロを使って実装する
#define SYSCALL(name)                                \
    Result name(                                     \
        uint64_t arg1, uint64_t arg2, uint64_t arg3, \
        uint64_t arg4, uint64_t arg5, uint64_t arg6)

    /// 背景に文字列表示
    /// arg1 : LogLevel
    /// arg2 s : 表示したい文字列へのポインタ
    SYSCALL(LogString) {
        if (arg1 != kError && arg1 != kWarn && arg1 != kInfo && arg1 != kDebug) {
            return {0, EPERM};
        }
        const char* s = reinterpret_cast<const char*>(arg2);
        const auto len = strlen(s);
        if (len > 1024) {
            return {0, E2BIG};
        }
        Log(static_cast<LogLevel>(arg1), "%s", s);
        return {len, 0};
    }

    /// ターミナルへの文字列表示
    /// arg1 fd : ファイルディスクリプタ番号。1はターミナルを表す番号とする
    /// arg2 s : 表示したい文字列へのポインタ
    /// arg3 len : null文字を含まないバイト数
    SYSCALL(PutString) {
        const auto fd = arg1;
        const char* s = reinterpret_cast<const char*>(arg2);
        const auto len = arg3;
        if (len > 1024) {
            return {0, E2BIG};
        }

        if (fd == 1) {
            // 現在実行中のタスク -> PutStringをコールしたアプリ、が動作するターミナルタスク
            const auto task_id = g_task_manager->CurrentTask().ID();
            (*g_terminals)[task_id]->Print(s, len);
            return {len, 0};
        }
        return {0, EBADF};
    }

    /// アプリ終了
    /// arg1 : 終了時コード
    SYSCALL(Exit) {
        __asm__("cli");
        auto& task = g_task_manager->CurrentTask();
        __asm__("sti");
        return {task.OSStackPointer(), static_cast<int>(arg1)};
    }
#undef SYSCALL

} // namespace syscall

using SyscallFuncType = syscall::Result(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/// システムコールの（関数ポインタ）テーブル
/// この添字に0x80000000を足した値をシステムコール番号とする
extern "C" std::array<SyscallFuncType*, 3> g_syscall_table{
    /* 0x00 */ syscall::LogString,
    /* 0x01 */ syscall::PutString,
    /* 0x02 */ syscall::Exit,
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
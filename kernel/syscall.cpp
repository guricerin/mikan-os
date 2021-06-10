#include "syscall.hpp"

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>

#include "app_event.hpp"
#include "asmfunc.h"
#include "font.hpp"
#include "keyboard.hpp"
#include "logger.hpp"
#include "msr.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "timer.hpp"

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

    /// ウィンドウを開く
    SYSCALL(OpenWindow) {
        const int w = arg1, h = arg2, x = arg3, y = arg4;
        const auto title = reinterpret_cast<const char*>(arg5);
        const auto win = std::make_shared<TopLevelWindow>(w, h, g_screen_config.pixel_format, title);

        __asm__("cli");
        const auto layer_id = g_layer_manager->NewLayer()
                                  .SetWindow(win)
                                  .SetDraggable(true)
                                  .Move({x, y})
                                  .ID();
        g_active_layer->Activate(layer_id);

        // アプリのウィンドウに入力したキーがターミナルタスクに送信されるようにする
        const auto task_id = g_task_manager->CurrentTask().ID();
        g_layer_task_map->insert(std::make_pair(layer_id, task_id));
        __asm__("sti");

        return {layer_id, 0};
    }

    namespace {
        /// 指定レイヤIDのウィンドウにfを行う
        /// layer_id_falgs : 上位32bitを再描画フラグ（0bit目が1なら再描画しない）、下位32bitをレイヤID
        template <class Func, class... Args>
        Result DoWinFunc(Func f, uint64_t layer_id_flags, Args... args) {
            const uint32_t layer_flags = layer_id_flags >> 32;
            const unsigned int layer_id = layer_id_flags & 0xffffffff;

            __asm__("cli");
            auto layer = g_layer_manager->FindLayer(layer_id);
            __asm__("sti");
            if (layer == nullptr) {
                return {0, EBADF};
            }

            const auto res = f(*layer->GetWindow(), args...);
            if (res.error) {
                return res;
            }

            if ((layer_flags & 1) == 0) {
                __asm__("cli");
                g_layer_manager->Draw(layer_id);
                __asm__("sti");
            }

            return res;
        }
    } // namespace

    /// ウィンドウに文字列を表示
    SYSCALL(WinWriteString) {
        return DoWinFunc(
            [](Window& win, int x, int y, unsigned int color, const char* s) {
                WriteString(*win.Writer(), {x, y}, s, ToColor(color));
                return Result{0, 0};
            },
            arg1, arg2, arg3, arg4, reinterpret_cast<const char*>(arg5));
    }

    /// ウィンドウの指定領域を塗りつぶす
    SYSCALL(WinFillRectangle) {
        return DoWinFunc(
            [](Window& win, int x, int y, int w, int h, uint32_t color) {
                FillRectangle(*win.Writer(), {x, y}, {w, h}, ToColor(color));
                return Result{0, 0};
            },
            arg1, arg2, arg3, arg4, arg5, arg6);
    }

    SYSCALL(GetCurrentTick) {
        return {g_timer_manager->CurrentTick(), kTimerFreq};
    }

    /// 指定レイヤを再描画するだけ
    SYSCALL(WinRedraw) {
        return DoWinFunc(
            [](Window&) {
                return Result{0, 0};
            },
            arg1);
    }

    /// 指定ウィンドウの指定の2点間に直線を引く
    SYSCALL(WinDrawLine) {
        return DoWinFunc(
            [](Window& win, int x0, int y0, int x1, int y1, uint32_t color) {
                auto sign = [](int x) {
                    return (x > 0) ? 1 : (x < 0) ? -1
                                                 : 0;
                };
                // 横、縦の変位
                // + sign()の補正がないと(x1, y1)を含まないような直線になってしまう
                const int dx = x1 - x0 + sign(x1 - x0);
                const int dy = y1 - y0 + sign(y1 - y0);

                // 2点が等しい場合
                if (dx == 0 && dy == 0) {
                    win.Writer()->Write({x0, y0}, ToColor(color));
                    return Result{0, 0};
                }

                // 切り捨て
                const auto floord = static_cast<double (*)(double)>(floor);
                // 切り上げ
                const auto ceild = static_cast<double (*)(double)>(ceil);

                if (abs(dx) >= abs(dy)) { // 傾きが1以下（水平に近い）の場合、x軸に沿って点を描画
                    if (dx < 0) {
                        std::swap(x0, x1);
                        std::swap(y0, y1);
                    }
                    const auto roundish = y1 >= y0 ? floord : ceild;
                    // 傾き
                    const double m = static_cast<double>(dy) / dx;
                    for (int x = x0; x <= x1; x++) {
                        const int y = roundish(m * (x - x0) + y0);
                        win.Writer()->Write({x, y}, ToColor(color));
                    }
                } else { // 傾きが1より大きい（垂直に近い）の場合、y軸に沿って点を描画
                    if (dy < 0) {
                        std::swap(x0, x1);
                        std::swap(y0, y1);
                    }
                    const auto roundish = x1 >= x0 ? floord : ceild;
                    const double m = static_cast<double>(dx) / dy;
                    for (int y = y0; y <= y1; y++) {
                        const int x = roundish(m * (y - y0) + x0);
                        win.Writer()->Write({x, y}, ToColor(color));
                    }
                }
                return Result{0, 0};
            },
            arg1, arg2, arg3, arg4, arg5, arg6);
    }

    /// 指定レイヤのウィンドウを削除
    SYSCALL(CloseWindow) {
        const unsigned int layer_id = arg1 & 0xffffffff;
        const auto layer = g_layer_manager->FindLayer(layer_id);

        if (layer == nullptr) {
            return {EBADF, 0};
        }

        // 削除する前に再描画範囲を取得しておく
        const auto layer_pos = layer->GetPosition();
        const auto win_size = layer->GetWindow()->Size();

        __asm__("cli");
        g_active_layer->Activate(0);
        g_layer_manager->RemoveLayer(layer_id);
        g_layer_manager->Draw({layer_pos, win_size});
        g_layer_task_map->erase(layer_id);
        __asm__("sti");

        return {0, 0};
    }

    /// イベント取得
    /// アプリ側はイベントが来るまでOS側によって自動的にスリープ
    SYSCALL(ReadEvent) {
        // OS側のメモリ（仮想アドレス空間の前半部）が指定されていたらエラーにする
        if (arg1 < 0x8000000000000000) {
            return {0, EFAULT};
        }
        const auto app_events = reinterpret_cast<AppEvent*>(arg1);
        const size_t len = arg2;

        __asm__("cli");
        // 実行中のタスク -> ターミナルタスク
        auto& task = g_task_manager->CurrentTask();
        __asm__("sti");
        size_t i = 0;

        while (i < len) {
            __asm__("cli");
            // アプリに対するキー入力はターミナルタスクのメッセージキューから受け取る
            auto msg = task.ReceiveMessage();
            if (!msg && i == 0) {
                task.Sleep();
                continue;
            }
            __asm__("sti");

            if (!msg) {
                break;
            }

            switch (msg->type) {
            case Message::kKeyPush:
                // Ctrl + Q で終了
                if (msg->arg.keyboard.keycode == 20 /* Q key */
                    && msg->arg.keyboard.modifier & (kLControlBitMask | kRControlBitMask)) {
                    app_events[i].type = AppEvent::kQuit;
                    i++;
                }
                break;
            case Message::kMouseMove:
                // 型変換
                app_events[i].type = AppEvent::kMouseMove;
                app_events[i].arg.mouse_move.x = msg->arg.mouse_move.x;
                app_events[i].arg.mouse_move.y = msg->arg.mouse_move.y;
                app_events[i].arg.mouse_move.dx = msg->arg.mouse_move.dx;
                app_events[i].arg.mouse_move.dy = msg->arg.mouse_move.dy;
                app_events[i].arg.mouse_move.buttons = msg->arg.mouse_move.buttons;
                i++;
                break;
            case Message::kMouseButton:
                // 型変換
                app_events[i].type = AppEvent::kMouseButton;
                app_events[i].arg.mouse_button.x = msg->arg.mouse_button.x;
                app_events[i].arg.mouse_button.y = msg->arg.mouse_button.y;
                app_events[i].arg.mouse_button.press = msg->arg.mouse_button.press;
                app_events[i].arg.mouse_button.button = msg->arg.mouse_button.button;
                i++;
                break;
            default:
                Log(kInfo, "uncaught event type: %u\n", msg->type);
                break;
            }
        }

        return {i, 0};
    }
#undef SYSCALL

} // namespace syscall

/// 引数の上限は6つ
using SyscallFuncType = syscall::Result(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/// システムコールの（関数ポインタ）テーブル
/// この添字に0x80000000を足した値をシステムコール番号とする
extern "C" std::array<SyscallFuncType*, 11> g_syscall_table{
    /* 0x00 */ syscall::LogString,
    /* 0x01 */ syscall::PutString,
    /* 0x02 */ syscall::Exit,
    /* 0x03 */ syscall::OpenWindow,
    /* 0x04 */ syscall::WinWriteString,
    /* 0x05 */ syscall::WinFillRectangle,
    /* 0x06 */ syscall::GetCurrentTick,
    /* 0x07 */ syscall::WinRedraw,
    /* 0x08 */ syscall::WinDrawLine,
    /* 0x09 */ syscall::CloseWindow,
    /* 0x0a */ syscall::ReadEvent,
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
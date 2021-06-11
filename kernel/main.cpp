#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <limits>
#include <numeric>
#include <vector>

#include "acpi.hpp"
#include "asmfunc.h"
#include "console.hpp"
#include "fat.hpp"
#include "font.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "keyboard.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "message.hpp"
#include "mouse.hpp"
#include "paging.hpp"
#include "pci.hpp"
#include "segment.hpp"
#include "syscall.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "timer.hpp"
#include "usb/xhci/xhci.hpp"
#include "window.hpp"

int printk(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    char s[1024];
    int result = vsprintf(s, format, ap);
    va_end(ap);

    g_console->PutString(s);
    return result;
}

std::shared_ptr<TopLevelWindow> g_main_window;
unsigned int g_main_window_layer_id;
void InitializeMainWindow() {
    g_main_window = std::make_shared<TopLevelWindow>(160, 52, g_screen_config.pixel_format, "Hello Window");

    g_main_window_layer_id = g_layer_manager->NewLayer()
                                 .SetWindow(g_main_window)
                                 .SetDraggable(true)
                                 .Move({300, 100})
                                 .ID();
    g_layer_manager->UpDown(g_main_window_layer_id, std::numeric_limits<int>::max());
}

std::shared_ptr<TopLevelWindow> g_text_window;
unsigned int g_text_window_layer_id;
void InitializeTextWindow() {
    const int win_w = 160;
    const int win_h = 52;

    g_text_window = std::make_shared<TopLevelWindow>(win_w, win_h, g_screen_config.pixel_format, "Text Box Test");
    DrawTextbox(*g_text_window->InnerWriter(), {0, 0}, g_text_window->InnerSize());

    g_text_window_layer_id = g_layer_manager->NewLayer()
                                 .SetWindow(g_text_window)
                                 .SetDraggable(true)
                                 .Move({500, 100})
                                 .ID();

    g_layer_manager->UpDown(g_text_window_layer_id, std::numeric_limits<int>::max());
}

// テキストボックスに現在表示している文字数
int g_text_window_index;

void DrawTextCursor(bool visible) {
    const auto color = visible ? ToColor(0) : ToColor(0xffffff);
    const auto pos = Vector2D<int>{4 + 8 * g_text_window_index, 5};
    FillRectangle(*g_text_window->InnerWriter(), pos, {7, 15}, color);
}

void InputTextWindow(char input) {
    if (input == 0) {
        return;
    }

    auto pos = []() { return Vector2D<int>{4 + 8 * g_text_window_index, 6}; };

    const int max_chars = (g_text_window->InnerSize().x - 8) / 8 - 1;
    // 入力がバックスペースなら削除
    if (input == '\b' && g_text_window_index > 0) {
        DrawTextCursor(false);
        g_text_window_index--;
        FillRectangle(*g_text_window->InnerWriter(), pos(), {8, 16}, ToColor(0xffffff));
        DrawTextCursor(true);
    } else if (input >= ' ' && g_text_window_index < max_chars) {
        DrawTextCursor(false);
        WriteAscii(*g_text_window->InnerWriter(), pos(), input, ToColor(0));
        g_text_window_index++;
        DrawTextCursor(true);
    }

    g_layer_manager->Draw(g_text_window_layer_id);
}

alignas(16) uint8_t g_kernel_main_stack[1024 * 1024];

// ブートローダからフレームバッファの情報とメモリマップを受け取る
extern "C" void KernelMainNewStack(const FrameBufferConfig& frame_buffer_config,
                                   const MemoryMap& memory_map,
                                   const acpi::RSDP& acpi_table,
                                   void* volume_image) {
    // フレームバッファ
    InitializeGraphics(frame_buffer_config);
    // メモリマネージャーやレイヤーマネージャーを生成する前のデバッグ情報を表示したいので、それらより前にコンソールを生成
    InitializeConsole();

    printk("Welcome to MikanOS!\n");
    SetLogLevel(kWarn);

    // メモリ管理
    InitializeSegmentation();
    InitializePaging();
    InitializeMemoryManager(memory_map);
    InitializeTSS();
    // 割り込み
    InitializeInterrupt();

    // FATファイルシステム
    fat::Initialize(volume_image);

    // デバイス
    InitializePCI();

    // GUIレイヤー
    InitializeLayer();
    InitializeMainWindow();
    InitializeTextWindow();
    // 初回の描画は画面全体を対象にする
    g_layer_manager->Draw({{0, 0}, ScreenSize()});

    // タイマ
    acpi::Initialize(acpi_table);
    InitializeLAPICTimer();

    // テキストボックスのカーソル点滅
    const int kTextboxCursorTimer = 1;
    const int kTimer05sec = static_cast<int>(kTimerFreq * 0.5);
    // 0,5secでタイムアウトするタイマ
    g_timer_manager->AddTimer(Timer{kTimer05sec, kTextboxCursorTimer, kMainTaskID});
    bool textbox_cursor_visible = false;

    // システムコール
    InitializeSyscall();

    // マルチタスク
    InitializeTask();
    // このタスク（KernelMainStack()）
    Task& main_task = g_task_manager->CurrentTask();
    g_terminals = new std::map<uint64_t, Terminal*>;
    const uint64_t task_terminal_id = g_task_manager->NewTask()
                                          .InitContext(TaskTerminal, 0)
                                          .Wakeup()
                                          .ID();

    // USBデバイス
    // xHCIは初期化するとすぐに割り込みが発生するので、タスク機能を初期化してからにする
    usb::xhci::Initialize();
    InitializeKeyboard();
    InitializeMouse();

    char str[128];
    // 割り込みイベントループ
    while (1) {
        // clear interrupt : 割り込みを無効化
        // データ競合の回避のため（キューの操作中に割り込みさせない）
        __asm__("cli");
        const auto tick = g_timer_manager->CurrentTick();
        // set interrupt : 割り込みを有効化
        __asm__("sti");
        // 割り込みが発生すると、この次の行から処理を再開

        sprintf(str, "%010lu", tick);
        FillRectangle(*g_main_window->InnerWriter(), {20, 4}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
        WriteString(*g_main_window->InnerWriter(), {20, 4}, str, {0, 0, 0});
        // カウンタの表示はメインウィンドウだけを再描画
        g_layer_manager->Draw(g_main_window_layer_id);

        __asm__("cli");
        auto msg = main_task.ReceiveMessage();
        if (!msg) {
            // メインタスクは他タスクより優先度が高いが、割り込みイベントがこない限りは眠らせる
            main_task.Sleep();
            __asm__("sti");
            continue;
        }
        __asm__("sti");

        switch (msg->type) {
        case Message::kInterruptXHCI:
            usb::xhci::ProcessEvents();
            break;
        case Message::kTimerTimeout:
            // カーソル点滅タイマがタイムアウトした場合
            if (msg->arg.timer.value == kTextboxCursorTimer) {
                __asm__("cli");
                g_timer_manager->AddTimer(Timer{msg->arg.timer.timeout + kTimer05sec, kTextboxCursorTimer, kMainTaskID});
                __asm__("sti");
                textbox_cursor_visible = !textbox_cursor_visible;
                DrawTextCursor(textbox_cursor_visible);
                g_layer_manager->Draw(g_text_window_layer_id);

                __asm__("cli");
                g_task_manager->SendMessage(task_terminal_id, *msg);
                __asm__("sti");
            }
            break;
        case Message::kKeyPush:
            if (auto act = g_active_layer->GetActive(); act == g_text_window_layer_id) {
                // キーの二重入力を防ぐ
                if (msg->arg.keyboard.press) {
                    InputTextWindow(msg->arg.keyboard.ascii);
                }
            } else {
                // アクティブなレイヤIDからタスクを検索し、そのタスクにメッセージを通知
                __asm__("cli");
                auto task_it = g_layer_task_map->find(act);
                __asm__("sti");
                if (task_it != g_layer_task_map->end()) {
                    __asm__("cli");
                    g_task_manager->SendMessage(task_it->second, *msg);
                    __asm__("sti");
                } else {
                    printk("key push no handled: keycode %02x, ascii %02x\n",
                           msg->arg.keyboard.keycode,
                           msg->arg.keyboard.ascii);
                }
            }
            break;
        case Message::kLayer:
            // 描画中の割り込みは許可しておく
            // 描画処理は低優先度な割にリソースを食うため、割り込みを禁止すると取りこぼしが発生するから
            ProcessLayerMessage(*msg);
            __asm__("cli");
            // 送信元タスクに描画終了を通知
            g_task_manager->SendMessage(msg->src_task, Message{Message::kLayerFinish});
            __asm__("sti");
            break;
        default:
            Log(kError, "Unknown message type: %d\n", msg->type);
        }
    }
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}

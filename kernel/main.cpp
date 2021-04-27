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

std::shared_ptr<Window> g_main_window;
unsigned int g_main_window_layer_id;
void InitializeMainWindow() {
    g_main_window = std::make_shared<Window>(160, 52, g_screen_config.pixel_format);
    DrawWindow(*g_main_window->Writer(), "Hello Window");

    g_main_window_layer_id = g_layer_manager->NewLayer()
                                 .SetWindow(g_main_window)
                                 .SetDraggable(true)
                                 .Move({300, 100})
                                 .ID();
    g_layer_manager->UpDown(g_main_window_layer_id, std::numeric_limits<int>::max());
}

std::shared_ptr<Window> g_text_window;
unsigned int g_text_window_layer_id;
void InitializeTextWindow() {
    const int win_w = 160;
    const int win_h = 52;

    g_text_window = std::make_shared<Window>(win_w, win_h, g_screen_config.pixel_format);
    DrawWindow(*g_text_window->Writer(), "Text Box Test");
    DrawTextbox(*g_text_window->Writer(), {4, 24}, {win_w - 8, win_h - 24 - 4});

    g_text_window_layer_id = g_layer_manager->NewLayer()
                                 .SetWindow(g_text_window)
                                 .SetDraggable(true)
                                 .Move({350, 200})
                                 .ID();

    g_layer_manager->UpDown(g_text_window_layer_id, std::numeric_limits<int>::max());
}

// テキストボックスに現在表示している文字数
int g_text_window_index;

void DrawTextCursor(bool visible) {
    const auto color = visible ? ToColor(0) : ToColor(0xffffff);
    const auto pos = Vector2D<int>{8 + 8 * g_text_window_index, 24 + 5};
    FillRectangle(*g_text_window->Writer(), pos, {7, 15}, color);
}

void InputTextWindow(char input) {
    if (input == 0) {
        return;
    }

    auto pos = []() { return Vector2D<int>{8 + 8 * g_text_window_index, 24 + 6}; };

    const int max_chars = (g_text_window->Width() - 16) / 8;
    // 入力がバックスペースなら削除
    if (input == '\b' && g_text_window_index > 0) {
        DrawTextCursor(false);
        g_text_window_index--;
        FillRectangle(*g_text_window->Writer(), pos(), {8, 16}, ToColor(0xffffff));
        DrawTextCursor(true);
    } else if (input >= ' ' && g_text_window_index < max_chars) {
        DrawTextCursor(false);
        WriteAscii(*g_text_window->Writer(), pos(), input, ToColor(0));
        g_text_window_index++;
        DrawTextCursor(true);
    }

    g_layer_manager->Draw(g_text_window_layer_id);
}

/// 割り込みキュー
std::deque<Message>* g_main_queue;

alignas(16) uint8_t g_kernel_main_stack[1024 * 1024];

// ブートローダからフレームバッファの情報とメモリマップを受け取る
extern "C" void KernelMainNewStack(const FrameBufferConfig& frame_buffer_config,
                                   const MemoryMap& memory_map,
                                   const acpi::RSDP& acpi_table) {
    // GUI
    InitializeGraphics(frame_buffer_config);
    // メモリマネージャーやレイヤーマネージャーを生成する前のデバッグ情報を表示したいので、それらより前にコンソールを生成
    InitializeConsole();

    printk("Welcome to MikanOS!\n");
    SetLogLevel(kWarn);

    // メモリ管理
    InitializeSegmentation();
    InitializePaging();
    InitializeMemoryManager(memory_map);
    // 割り込み
    ::g_main_queue = new std::deque<Message>(32);
    InitializeInterrupt(g_main_queue);

    // デバイス
    InitializePCI();
    usb::xhci::Initialize();

    // GUIレイヤー
    InitializeLayer();
    InitializeMainWindow();
    InitializeTextWindow();
    InitializeMouse();
    // 初回の描画は画面全体を対象にする
    g_layer_manager->Draw({{0, 0}, ScreenSize()});

    // タイマ
    acpi::Initialize(acpi_table);
    InitializeLAPICTimer(*g_main_queue);

    // キーボード
    InitializeKeyboard(*g_main_queue);

    // テキストボックスのカーソル点滅
    const int kTextboxCursorTimer = 1;
    const int kTimer05sec = static_cast<int>(kTimerFreq * 0.5);
    __asm__("cli");
    // 0,5secでタイムアウトするタイマ
    g_timer_manager->AddTimer(Timer{kTimer05sec, kTextboxCursorTimer});
    __asm__("sti");
    bool textbox_cursor_visible = false;

    char str[128];
    // 割り込みイベントループ
    while (1) {
        // データ競合回避のため、タイマ割り込み回数の取得処理中は割り込みを無効化
        __asm__("cli");
        const auto tick = g_timer_manager->CurrentTick();
        __asm__("sti");

        sprintf(str, "%010lu", tick);
        FillRectangle(*g_main_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
        WriteString(*g_main_window->Writer(), {24, 28}, str, {0, 0, 0});
        // カウンタの表示はメインウィンドウだけを再描画
        g_layer_manager->Draw(g_main_window_layer_id);

        // clear interrupt : 割り込みを無効化
        // データ競合の回避のため（キューの操作中に割り込みさせない）
        __asm__("cli");
        if (g_main_queue->size() == 0) {
            // set interrupt : 割り込みを有効化
            __asm__("sti\t\nhlt");
            // 割り込みが発生すると、この次の行（ここではcontinue）から処理を再開
            continue;
        }

        Message msg = g_main_queue->front();
        g_main_queue->pop_front();
        __asm__("sti");

        switch (msg.type) {
        case Message::kInterruptXHCI:
            usb::xhci::ProcessEvents();
            break;
        case Message::kTimerTimeout:
            // カーソル点滅タイマがタイムアウトした場合
            if (msg.arg.timer.value == kTextboxCursorTimer) {
                __asm__("cli");
                g_timer_manager->AddTimer(Timer{msg.arg.timer.timeout + kTimer05sec, kTextboxCursorTimer});
                __asm__("sti");
                textbox_cursor_visible = !textbox_cursor_visible;
                DrawTextCursor(textbox_cursor_visible);
                g_layer_manager->Draw(g_text_window_layer_id);
            }
            break;
        case Message::kKeyPush:
            InputTextWindow(msg.arg.keyboard.ascii);
            break;
        default:
            Log(kError, "Unknown message type: %d\n", msg.type);
        }
    }
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <deque>
#include <limits>
#include <numeric>
#include <vector>

#include "asmfunc.h"
#include "console.hpp"
#include "font.hpp"
#include "frame_buffer.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "message.hpp"
#include "mouse.hpp"
#include "paging.hpp"
#include "pci.hpp"
#include "queue.hpp"
#include "segment.hpp"
#include "timer.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/device.hpp"
#include "usb/memory.hpp"
#include "usb/xhci/trb.hpp"
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

/// 割り込みキュー
std::deque<Message>* g_main_queue;

alignas(16) uint8_t g_kernel_main_stack[1024 * 1024];

// ブートローダからフレームバッファの情報とメモリマップを受け取る
extern "C" void KernelMainNewStack(const FrameBufferConfig& frame_buffer_config,
                                   const MemoryMap& memory_map) {
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
    InitializeMouse();
    // 初回の描画は画面全体を対象にする
    g_layer_manager->Draw({{0, 0}, ScreenSize()});

    // タイマの初期化
    // InitializeLAPICTimer();

    char str[128];
    unsigned int count = 0;
    // 割り込みイベントループ
    while (1) {
        count++;
        sprintf(str, "%010u", count);
        FillRectangle(*g_main_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
        WriteString(*g_main_window->Writer(), {24, 28}, str, {0, 0, 0});
        // カウンタの表示はメインウィンドウだを再描画
        g_layer_manager->Draw(g_main_window_layer_id);

        // clear interrupt : 割り込みを無効化
        // データ競合の回避（キューの操作中に割り込みさせない）
        __asm__("cli");
        if (g_main_queue->size() == 0) {
            // set interrupt : 割り込みを有効化
            __asm__("sti");
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
        default:
            Log(kError, "Unknown message type: %d\n", msg.type);
        }
    }
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}

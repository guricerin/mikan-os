#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <array>
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

char g_pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* g_pixel_writer;

char g_console_buf[sizeof(Console)];
Console* g_console;

int printk(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    char s[1024];
    int result = vsprintf(s, format, ap);
    va_end(ap);

    StartLAPICTimer();
    g_console->PutString(s);
    auto elapsed = LAPICTimerElapsed();
    StopLAPICTimer();

    sprintf(s, "[%9d]", elapsed);
    g_console->PutString(s);
    return result;
}

char g_memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager* g_memory_manager;

unsigned int g_mouse_layer_id;
Vector2D<int> g_screen_size;
Vector2D<int> g_mouse_position;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
    // マウスの移動範囲を画面内に制限
    auto new_pos = g_mouse_position + Vector2D<int>{displacement_x, displacement_y};
    new_pos = ElementMin(new_pos, g_screen_size + Vector2D<int>{-1, -1});
    g_mouse_position = ElementMax(new_pos, {0, 0});
    g_layer_manager->Move(g_mouse_layer_id, g_mouse_position);
    g_layer_manager->Draw();
}

void SwitchEhci2Xhci(const pci::Device& xhc_device) {
    bool intel_ehc_exist = false;
    for (int i = 0; i < pci::g_num_device; ++i) {
        if (pci::g_devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) /* EHCI */ &&
            pci::ReadVendorId(pci::g_devices[i]) == 0x8086) {
            intel_ehc_exist = true;
            break;
        }
    }
    if (!intel_ehc_exist) {
        return;
    }

    uint32_t superspeed_ports = pci::ReadConfReg(xhc_device, 0xdc); // USB3PRM
    pci::WriteConfReg(xhc_device, 0xd8, superspeed_ports);          // USB3_PSSEN
    uint32_t ehci2xhci_ports = pci::ReadConfReg(xhc_device, 0xd4);  // XUSB2PRM
    pci::WriteConfReg(xhc_device, 0xd0, ehci2xhci_ports);           // XUSB2PR
    Log(kDebug, "SwitchEhci2Xhci: SS = %02, xHCI = %02x\n",
        superspeed_ports, ehci2xhci_ports);
}

/// xHCIホストコントローラ
usb::xhci::Controller* g_xhc;

/// 割り込みメッセージ
struct Message {
    enum Type {
        kInterruptXHCI,
    } type;
};

/// 割り込みキュー
ArrayQueue<Message>* g_main_queue;

/// xHCI用割り込みハンドラ
__attribute__((interrupt)) void IntHandlerXHCI(InterruptFrame* frame) {
    g_main_queue->Push(Message{Message::kInterruptXHCI});
    NotifyEndOfInterrupt();
}

alignas(16) uint8_t g_kernel_main_stack[1024 * 1024];

// ブートローダからフレームバッファの情報とメモリマップを受け取る
extern "C" void KernelMainNewStack(const FrameBufferConfig& frame_buffer_config,
                                   const MemoryMap& memory_map) {
    switch (frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
        g_pixel_writer = new (g_pixel_writer_buf) RGBResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    case kPixelBGRResv8BitPerColor:
        g_pixel_writer = new (g_pixel_writer_buf) BGRResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    }

    // ピクセル描画
    DrawDesktop(*g_pixel_writer);

    // メモリマネージャーやレイヤーマネージャーを生成する前のデバッグ情報を表示したいので、それらより前にコンソールを生成
    g_console = new (g_console_buf) Console{kDesktopFGColor, kDesktopBGColor};
    g_console->SetWriter(g_pixel_writer);
    printk("Welcome to MikanOS!\n");
    SetLogLevel(kWarn);

    // タイマの初期化
    InitializeLAPICTimer();

    // セグメントの設定（GDTを再構築）
    SetupSegments();

    const uint16_t kernel_cs = 1 << 3;
    const uint16_t kernel_ss = 2 << 3;
    // ヌルディスクリプタにする
    SetDSAll(0);
    SetCSSS(kernel_cs, kernel_ss);

    // ページテーブル設定
    SetupIdentityPageTable();

    ::g_memory_manager = new (g_memory_manager_buf) BitmapMemoryManager;
    // メモリマネージャーにUEFIのメモリマップを伝える
    const auto memory_map_base = reinterpret_cast<uintptr_t>(memory_map.buffer);
    uintptr_t available_end = 0;
    for (uintptr_t iter = memory_map_base;
         iter < memory_map_base + memory_map.map_size;
         iter += memory_map.descriptor_size) {
        auto desc = reinterpret_cast<MemoryDescriptor*>(iter);
        if (available_end < desc->physical_start) {
            g_memory_manager->MarkAllocated(
                FrameID(available_end / kBytesPerFrame),
                (desc->physical_start - available_end) / kBytesPerFrame);
        }

        const auto physical_end = desc->physical_start + desc->number_of_pages * kUEFIPageSize;
        if (IsAvailable(static_cast<MemoryType>(desc->type))) {
            available_end = physical_end;
        } else {
            g_memory_manager->MarkAllocated(
                FrameID{desc->physical_start / kBytesPerFrame},
                desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
        }
    }
    g_memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end / kBytesPerFrame});

    if (auto err = InitializeHeap(*g_memory_manager)) {
        Log(kError, "failed to allocate pages: %s at %s:%d",
            err.Name(), err.File(), err.Line());
        exit(1);
    }

    std::array<Message, 32> main_queue_data;
    ArrayQueue<Message> main_queue(main_queue_data);
    ::g_main_queue = &main_queue;

    // PCIデバイスを列挙
    auto err = pci::ScanAllBus();
    Log(kDebug, "ScanAllBus: %s\n", err.Name());
    Log(kDebug, "pci::num_device: %d\n", pci::g_num_device);
    for (int i = 0; i < pci::g_num_device; i++) {
        const auto& device = pci::g_devices[i];
        auto vendor_id = pci::ReadVendorId(device);
        auto class_code = pci::ReadClassCode(device.bus, device.device, device.function);
        Log(kDebug, "%d.%d.%d: vend %04x, class %08x, head %02x\n",
            device.bus, device.device, device.function, vendor_id, class_code, device.header_type);
    }

    // intel製を優先してxHCを探す
    pci::Device* xhc_device = nullptr;
    for (int i = 0; i < pci::g_num_device; i++) {
        if (pci::g_devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)) {
            xhc_device = &pci::g_devices[i];

            if (0x8086 == pci::ReadVendorId(*xhc_device)) {
                break;
            }
        }
    }
    if (xhc_device) {
        Log(kInfo, "xHC has been found: %d.%d.%d\n",
            xhc_device->bus, xhc_device->device, xhc_device->function);
    } else {
        Log(kError, "Error! xHC has been not found: %d.%d.%d\n",
            xhc_device->bus, xhc_device->device, xhc_device->function);
    }

    // IDTをCPUに登録
    const uint16_t cs = GetCS();
    SetIDTEntry(g_idt[InterruptVector::kXHCI],
                MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(IntHandlerXHCI),
                cs);
    LoadIDT(sizeof(g_idt) - 1, reinterpret_cast<uintptr_t>(&g_idt[0]));

    // MSI割り込みを有効化
    // このプログラムが動作しているCPUコア（Bootstrap Processor）の固有番号
    const uint8_t bsp_local_apic_id = *reinterpret_cast<const uint32_t*>(0xfee00020) >> 24;
    pci::ConfigureMSIFixedDestination(
        *xhc_device, bsp_local_apic_id,
        pci::MSITriggerMode::kLevel, pci::MSIDeliveryMode::kFixed,
        InterruptVector::kXHCI, 0);

    // xHCを制御するレジスタ群はメモリマップドIOなので、そのアドレスを取得
    const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_device, 0);
    Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());
    const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
    Log(kDebug, "xHC mmio_base = %08lx\n", xhc_mmio_base);

    // xHC初期化
    usb::xhci::Controller xhc{xhc_mmio_base};
    if (pci::ReadVendorId(*xhc_device) == 0x8086) {
        Log(kDebug, "%s, %d\n", __FILE__, __LINE__);
        SwitchEhci2Xhci(*xhc_device);
    } else {
        Log(kDebug, "%s, %d\n", __FILE__, __LINE__);
    }
    {
        auto err = xhc.Initialize();
        Log(kInfo, "xhc.Initialize: %s\n", err.Name());
    }

    // xHC起動
    Log(kInfo, "xHC starting\n");
    xhc.Run();

    ::g_xhc = &xhc;

    // すべてのUSBポートを探索し、何かが接続されているポートの設定を行う
    usb::HIDMouseDriver::default_observer = MouseObserver;
    for (int i = 1; i <= xhc.MaxPorts(); i++) {
        auto port = xhc.PortAt(i);
        Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());

        if (port.IsConnected()) {
            if (auto err = ConfigurePort(xhc, port)) {
                Log(kError, "failed to configure port: %s at %s:%d\n",
                    err.Name(), err.File(), err.Line());
                continue;
            }
        }
    }

    // GUI
    g_screen_size.x = frame_buffer_config.horizontal_resolution;
    g_screen_size.y = frame_buffer_config.vertical_resolution;

    // 背景ウィンドウ
    auto bg_window = std::make_shared<Window>(g_screen_size.x, g_screen_size.y, frame_buffer_config.pixel_format);
    auto bg_writer = bg_window->Writer();
    DrawDesktop(*bg_writer);
    // レイヤーマネージャーの準備が整ったので、コンソールをレイヤーの仕組みに載せ替える
    g_console->SetWindow(bg_window);

    // マウスカーソル描画
    auto mouse_window = std::make_shared<Window>(kMouseCursorWidth, kMouseCursorHeight, frame_buffer_config.pixel_format);
    mouse_window->SetTransparentColor(kMouseTransparentColor);
    DrawMouseCursor(mouse_window->Writer(), {0, 0});
    g_mouse_position = {200, 200};

    // 本物のフレームバッファー
    FrameBuffer screen;
    if (auto err = screen.Initailize(frame_buffer_config)) {
        Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
    }

    g_layer_manager = new LayerManager;
    g_layer_manager->SetWriter(&screen);

    auto bg_layer_id = g_layer_manager->NewLayer()
                           .SetWindow(bg_window)
                           .Move({0, 0})
                           .ID();

    g_mouse_layer_id = g_layer_manager->NewLayer()
                           .SetWindow(mouse_window)
                           .Move(g_mouse_position)
                           .ID();

    g_layer_manager->UpDown(bg_layer_id, 0);
    g_layer_manager->UpDown(g_mouse_layer_id, 1);
    g_layer_manager->Draw();

    // 割り込みイベントループ
    while (1) {
        // clear interrupt : 割り込みを無効化
        // データ競合の回避（キューの操作中に割り込みさせない）
        __asm__("cli");
        if (main_queue.Count() == 0) {
            // set interrupt : 割り込みを有効化
            __asm__("sti\n\thlt");
            // 割り込みが発生すると、この次の行（ここではcontinue）から処理を再開
            continue;
        }

        Message msg = main_queue.Front();
        main_queue.Pop();
        __asm__("sti");

        switch (msg.type) {
        case Message::kInterruptXHCI:
            while (xhc.PrimaryEventRing()->HasFront()) {
                if (auto err = ProcessEvent(xhc)) {
                    Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
                        err.Name(), err.File(), err.Line());
                }
            }
            break;
        default:
            Log(kError, "Unknown message type: %d\n", msg.type);
        }
    }
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}

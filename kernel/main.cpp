#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <numeric>
#include <vector>

#include "../MikanLoaderPkg/frame_buffer_config.h"
#include "console.hpp"
#include "font.hpp"
#include "graphics.hpp"
#include "logger.hpp"
#include "mouse.hpp"
#include "pci.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/device.hpp"
#include "usb/memory.hpp"
#include "usb/xhci/trb.hpp"
#include "usb/xhci/xhci.hpp"

const PixelColor g_desktop_bg_color{45, 118, 237};
const PixelColor g_desktop_fg_color{255, 255, 255};

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

    g_console->PutString(s);
    return result;
}

char g_mouse_cursor_buf[sizeof(MouseCursor)];
MouseCursor* g_mouse_cursor;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
    g_mouse_cursor->MoveRelative({displacement_x, displacement_y});
}

void SwitchEhci2Xhci(const pci::Device& xhc_device) {
    bool intel_ehc_exist = false;
    for (int i = 0; i < pci::g_num_device; ++i) {
        if (pci::g_devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) /* EHCI */ &&
            0x8086 == pci::ReadVendorId(pci::g_devices[i])) {
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

// ブートローダからフレームバッファの情報を受け取る
extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
    switch (frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
        g_pixel_writer = new (g_pixel_writer_buf) RGBResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    case kPixelBGRResv8BitPerColor:
        g_pixel_writer = new (g_pixel_writer_buf) BGRResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    }

    const int frame_width = frame_buffer_config.horizontal_resolution;
    const int frame_height = frame_buffer_config.vertical_resolution;
    // ピクセル描画
    // 背景
    FillRectangle(*g_pixel_writer, {0, 0}, {frame_width, frame_height - 50}, g_desktop_bg_color);
    // 下部のアイコンバー
    FillRectangle(*g_pixel_writer, {0, frame_height - 50}, {frame_width, 50}, {1, 8, 17});
    FillRectangle(*g_pixel_writer, {0, frame_height - 50}, {frame_width / 5, 50}, {80, 80, 80});
    // 左下のメニューアイコンっぽい図形
    DrawRectangle(*g_pixel_writer, {10, frame_height - 40}, {30, 30}, {160, 160, 160});

    g_console = new (g_console_buf) Console{*g_pixel_writer, g_desktop_fg_color, g_desktop_bg_color};
    printk("Welcome to MikanOS!\n");
    SetLogLevel(kWarn);

    // マウスカーソル描画
    g_mouse_cursor = new (g_mouse_cursor_buf) MouseCursor{
        g_pixel_writer, g_desktop_bg_color, {300, 200}};

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
        Log(kWarn, "xHC has been found: %d.%d.%d\n",
            xhc_device->bus, xhc_device->device, xhc_device->function);
    } else {
        Log(kError, "Error! xHC has been not found: %d.%d.%d\n",
            xhc_device->bus, xhc_device->device, xhc_device->function);
    }

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

    // すべてのUSBポートを探索し、何かが接続されているポートの設定を行う
    usb::HIDMouseDriver::default_observer = MouseObserver;
    for (int i = 1; i <= xhc.MaxPorts(); i++) {
        auto port = xhc.PortAt(i);
        Log(kDebug, "Port %d: IsConnected=%d\n", port.IsConnected());

        if (port.IsConnected()) {
            if (auto err = ConfigurePort(xhc, port)) {
                Log(kError, "failed to configure port: %s at %s:%d\n",
                    err.Name(), err.File(), err.Line());
                continue;
            }
        }
    }

    // ポーリング（xHCのイベント処理）
    while (1) {
        if (auto err = ProcessEvent(xhc)) {
            Log(kError, "Error while ProccessEvent: %s at %s:%d\n",
                err.Name(), err.File(), err.Line());
        }
    }

    while (1) {
        // CPU停止
        __asm__("hlt");
    }
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}

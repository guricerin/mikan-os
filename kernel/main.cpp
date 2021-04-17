#include "../MikanLoaderPkg/frame_buffer_config.h"
#include "font.hpp"
#include "graphics.hpp"
#include <cstddef>
#include <cstdint>

/** 配置new
 * この時点のカーネルにはメモリ管理機能がないので普通のnewは使えない
*/
void* operator new(size_t size, void* buf) {
    return buf;
}

void operator delete(void* obj) noexcept {
}

char g_pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* g_pixel_writer;

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

    // ピクセル描画
    // 画面全体を白くする
    for (int x = 0; x < frame_buffer_config.horizontal_resolution; x++) {
        for (int y = 0; y < frame_buffer_config.vertical_resolution; y++) {
            g_pixel_writer->Write(x, y, {255, 255, 255});
        }
    }
    // 一部を緑にする
    for (int x = 0; x < 200; x++) {
        for (int y = 0; y < 100; y++) {
            g_pixel_writer->Write(x, y, {0, 255, 0});
        }
    }

    WriteAscii(*g_pixel_writer, 50, 50, 'A', {0, 0, 0});
    WriteAscii(*g_pixel_writer, 58, 50, 'A', {0, 0, 0});

    while (1) {
        // CPU停止
        __asm__("hlt");
    }
}
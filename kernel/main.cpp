#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "../MikanLoaderPkg/frame_buffer_config.h"
#include "console.hpp"
#include "font.hpp"
#include "graphics.hpp"

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

const PixelColor g_desktop_bg_color{45, 118, 237};
const PixelColor g_desktop_fg_color{255, 255, 255};

const int g_mouse_cursor_width = 15;
const int g_mouse_cursor_height = 24;
const char g_mouse_cursor_shape[g_mouse_cursor_height][g_mouse_cursor_width + 1] = {
    "@              ",
    "@@             ",
    "@.@            ",
    "@..@           ",
    "@...@          ",
    "@....@         ",
    "@.....@        ",
    "@......@       ",
    "@.......@      ",
    "@........@     ",
    "@.........@    ",
    "@..........@   ",
    "@...........@  ",
    "@............@ ",
    "@......@@@@@@@@",
    "@......@       ",
    "@....@@.@      ",
    "@...@ @.@      ",
    "@..@   @.@     ",
    "@.@    @.@     ",
    "@@      @.@    ",
    "@       @.@    ",
    "         @.@   ",
    "         @@@   ",
};

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

    // マウスカーソル描画
    for (int dy = 0; dy < g_mouse_cursor_height; dy++) {
        for (int dx = 0; dx < g_mouse_cursor_width; dx++) {
            if (g_mouse_cursor_shape[dy][dx] == '@') {
                g_pixel_writer->Write(200 + dx, 100 + dy, {0, 0, 0});
            } else if (g_mouse_cursor_shape[dy][dx] == '.') {
                g_pixel_writer->Write(200 + dx, 100 + dy, {255, 255, 255});
            }
        }
    }

    while (1) {
        // CPU停止
        __asm__("hlt");
    }
}
#include "../MikanLoaderPkg/frame_buffer_config.h"
#include <cstdint>

struct PixelColor {
    uint8_t r, g, b;
};

/** 1つの点を描画。 
 * @retval 0 成功
 * @retval 非0 失敗
*/
int WritePixel(const FrameBufferConfig& config, int x, int y, const PixelColor& color) {
    const int pixel_position = config.pixels_per_scan_line * y + x;
    // 1ピクセルは4バイト（rgbの24bit + 予約領域の8bit）
    uint8_t* p = &config.frame_buffer[4 * pixel_position];

    switch (config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
        p[0] = color.r;
        p[1] = color.g;
        p[2] = color.b;
        break;
    case kPixelBGRResv8BitPerColor:
        p[0] = color.b;
        p[1] = color.g;
        p[2] = color.r;
        break;
    default:
        return -1;
    }
    return 0;
}

// ブートローダからフレームバッファの情報を受け取る
extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
    // ピクセル描画
    // 画面全体を白くする
    for (int x = 0; x < frame_buffer_config.horizontal_resolution; x++) {
        for (int y = 0; y < frame_buffer_config.vertical_resolution; y++) {
            WritePixel(frame_buffer_config, x, y, {255, 255, 255});
        }
    }
    // 一部を緑にする
    for (int x = 0; x < 200; x++) {
        for (int y = 0; y < 100; y++) {
            WritePixel(frame_buffer_config, 100 + x, 100 + y, {0, 255, 0});
        }
    }

    while (1) {
        // CPU停止
        __asm__("hlt");
    }
}
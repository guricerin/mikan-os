#include "../MikanLoaderPkg/frame_buffer_config.h"
#include <cstddef>
#include <cstdint>

struct PixelColor {
    uint8_t r, g, b;
};

class PixelWriter {
private:
    const FrameBufferConfig& _config;

public:
    PixelWriter(const FrameBufferConfig& config) : _config(config) {
    }
    virtual ~PixelWriter() = default;
    virtual void Write(int x, int y, const PixelColor& color) = 0;

protected:
    uint8_t* PixelAt(int x, int y) {
        // 1ピクセルは4バイト（rgbの24bit + 予約領域の8bit）
        return _config.frame_buffer + 4 * (_config.pixels_per_scan_line * y + x);
    }
};

class RGBResv8BitPerColorPixelWriter : public PixelWriter {
public:
    using PixelWriter::PixelWriter;

    virtual void Write(int x, int y, const PixelColor& color) override {
        auto p = PixelAt(x, y);
        p[0] = color.r;
        p[1] = color.g;
        p[2] = color.b;
    }
};

class BGRResv8BitPerColorPixelWriter : public PixelWriter {
public:
    using PixelWriter::PixelWriter;

    virtual void Write(int x, int y, const PixelColor& color) override {
        auto p = PixelAt(x, y);
        p[0] = color.b;
        p[1] = color.g;
        p[2] = color.r;
    }
};

/** 1つの点を描画。 
 * @retval 0 成功
 * @retval 非0 失敗
*/
int WritePixel(const FrameBufferConfig& config, int x, int y, const PixelColor& color) {
    const int pixel_position = config.pixels_per_scan_line * y + x;
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

/** 配置new
 * この時点のカーネルにはメモリ管理機能がないので普通のnewは使えない
*/
void* operator new(size_t size, void* buf) {
    return buf;
}

void operator delete(void* obj) noexcept {
}

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

// ブートローダからフレームバッファの情報を受け取る
extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
    switch (frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
        pixel_writer = new (pixel_writer_buf) RGBResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    case kPixelBGRResv8BitPerColor:
        pixel_writer = new (pixel_writer_buf) BGRResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    }

    // ピクセル描画
    // 画面全体を白くする
    for (int x = 0; x < frame_buffer_config.horizontal_resolution; x++) {
        for (int y = 0; y < frame_buffer_config.vertical_resolution; y++) {
            pixel_writer->Write(x, y, {255, 255, 255});
        }
    }
    // 一部を緑にする
    for (int x = 0; x < 200; x++) {
        for (int y = 0; y < 100; y++) {
            pixel_writer->Write(x, y, {0, 255, 0});
        }
    }

    while (1) {
        // CPU停止
        __asm__("hlt");
    }
}
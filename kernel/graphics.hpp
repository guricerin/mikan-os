#pragma once

#include "frame_buffer_config.hpp"
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
        // 1ピクセルは4byte（rgbの24bit + 予約領域の8bit）
        return _config.frame_buffer + 4 * (_config.pixels_per_scan_line * y + x);
    }
};

class RGBResv8BitPerColorPixelWriter : public PixelWriter {
public:
    using PixelWriter::PixelWriter;
    virtual void Write(int x, int y, const PixelColor& color) override;
};

class BGRResv8BitPerColorPixelWriter : public PixelWriter {
public:
    using PixelWriter::PixelWriter;
    virtual void Write(int x, int y, const PixelColor& color) override;
};

template <typename T>
struct Vector2D {
    T x, y;

    template <typename U>
    Vector2D<T>& operator+=(const Vector2D<U>& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
};

void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos, const Vector2D<int>& size, const PixelColor& color);

void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos, const Vector2D<int>& size, const PixelColor& color);
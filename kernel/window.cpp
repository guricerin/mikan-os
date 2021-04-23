#include "window.hpp"

#include "logger.hpp"

Window::Window(int width, int height, PixelFormat shadow_format) : width_{width}, height_{height} {
    data_.resize(height);
    for (int y = 0; y < height; y++) {
        data_[y].resize(width);
    }

    FrameBufferConfig fb_config{};
    fb_config.frame_buffer = nullptr;
    fb_config.horizontal_resolution = width;
    fb_config.vertical_resolution = height;
    fb_config.pixel_format = shadow_format;

    if (auto err = shadow_buffer_.Initailize(fb_config)) {
        Log(kError, "failed to initialize shadow buffer: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
    }
}

void Window::DrawTo(FrameBuffer& dst, Vector2D<int> position) {
    if (!transparent_color_) {
        dst.Copy(position, shadow_buffer_);
        return;
    }

    const auto tc = transparent_color_.value();
    auto& writer = dst.Writer();
    for (int y = 0; y < Height(); y++) {
        for (int x = 0; x < Width(); x++) {
            const auto color = At(Vector2D<int>{x, y});
            if (color != tc) {
                writer.Write(position + Vector2D<int>{x, y}, color);
            }
        }
    }
}

void Window::SetTransparentColor(std::optional<PixelColor> color) {
    transparent_color_ = color;
}
/// このインスタンスに紐付いたWindowWriterを取得
Window::WindowWriter* Window::Writer() {
    return &writer_;
}

/// 指定した位置のピクセルを返す
const PixelColor& Window::At(Vector2D<int> pos) const {
    return data_[pos.y][pos.x];
}

void Window::Write(Vector2D<int> pos, PixelColor color) {
    data_[pos.y][pos.x] = color;
    shadow_buffer_.Writer().Write(pos, color);
}

int Window::Width() const {
    return width_;
}

int Window::Height() const {
    return height_;
}

void Window::Move(Vector2D<int> dst_pos, const Rectangle<int>& src) {
    shadow_buffer_.Move(dst_pos, src);
}
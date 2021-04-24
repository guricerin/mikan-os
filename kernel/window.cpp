#include "window.hpp"

#include "font.hpp"
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
    // 条件式は、描画領域が画面端を超えた際に反対側から飛び出るのを防ぐのを意味する
    for (int y = std::max(0, 0 - position.y);
         y < std::min(Height(), writer.Height() - position.y);
         y++) {
        for (int x = std::max(0, 0 - position.x);
             x < std::min(Width(), writer.Width() - position.x);
             x++) {
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

namespace {
    const int kCloseButtonWidth = 16;
    const int kCloseButtonHeight = 14;
    const char k_close_button[kCloseButtonHeight][kCloseButtonWidth + 1] = {
        "...............@",
        ".:::::::::::::$@",
        ".:::::::::::::$@",
        ".:::@@::::@@::$@",
        ".::::@@::@@:::$@",
        ".:::::@@@@::::$@",
        ".::::::@@:::::$@",
        ".:::::@@@@::::$@",
        ".::::@@::@@:::$@",
        ".:::@@::::@@::$@",
        ".:::::::::::::$@",
        ".:::::::::::::$@",
        ".$$$$$$$$$$$$$$@",
        "@@@@@@@@@@@@@@@@",
    };

    constexpr PixelColor ToColor(uint32_t c) {
        // rgbの順
        return {
            static_cast<uint8_t>((c >> 16) & 0xff),
            static_cast<uint8_t>((c >> 8) & 0xff),
            static_cast<uint8_t>(c & 0xff)};
    }
} // namespace

void DrawWindow(PixelWriter& writer, const char* title) {
    auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, uint32_t c) {
        FillRectangle(writer, pos, size, ToColor(c));
    };
    const auto win_w = writer.Width();
    const auto win_h = writer.Height();

    fill_rect({0, 0}, {win_w, 1}, 0x6c6c6);
    fill_rect({1, 1}, {win_w - 2, 1}, 0xffffff);
    fill_rect({0, 0}, {1, win_h}, 0x6c6c6);
    fill_rect({1, 1}, {1, win_h - 2}, 0xffffff);
    fill_rect({win_w - 2, 1}, {1, win_h - 2}, 0x848484);
    fill_rect({win_w - 1, 0}, {1, win_h}, 0x000000);
    fill_rect({2, 2}, {win_w - 4, win_h - 4}, 0x6c6c6);
    fill_rect({3, 3}, {win_w - 6, 18}, 0x000084);
    fill_rect({1, win_h - 2}, {win_w - 2, 1}, 0x848484);
    fill_rect({0, win_h - 1}, {win_w, 1}, 0x000000);

    WriteString(writer, {24, 4}, title, ToColor(0xffffff));

    for (int y = 0; y < kCloseButtonHeight; y++) {
        for (int x = 0; x < kCloseButtonWidth; x++) {
            PixelColor color = ToColor(0xffffff);

            if (k_close_button[y][x] == '@') {
                color = ToColor(0x000000);
            } else if (k_close_button[y][x] == '$') {
                color = ToColor(0x848484);
            } else if (k_close_button[y][x] == ':') {
                color = ToColor(0x6c6c6c);
            }
            writer.Write({win_w - 5 - kCloseButtonWidth + x, 5 + y}, color);
        }
    }
}
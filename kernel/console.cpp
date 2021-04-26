#include "console.hpp"

#include "font.hpp"
#include "layer.hpp"
#include <cstring>

Console::Console(const PixelColor& fg_color, const PixelColor& bg_color)
    : writer_{nullptr}, fg_color_{fg_color}, bg_color_{bg_color},
      buffer_{}, cursor_row_{0}, cursor_column_{0} {
}

void Console::PutString(const char* s) {
    while (*s) {
        if (*s == '\n') {
            NewLine();
        } else if (cursor_column_ < kColumns - 1) {
            WriteAscii(*writer_, Vector2D<int>{8 * cursor_column_, 16 * cursor_row_}, *s, fg_color_);
            buffer_[cursor_row_][cursor_column_] = *s;
            cursor_column_++;
        }
        s++;
    }
    if (g_layer_manager) {
        g_layer_manager->Draw(layer_id_);
    }
}

void Console::SetWriter(PixelWriter* writer) {
    if (writer_ == writer) {
        return;
    }
    writer_ = writer;
    window_.reset();
    Refresh();
}

void Console::SetWindow(const std::shared_ptr<Window>& window) {
    if (window == window_) {
        return;
    }
    window_ = window;
    writer_ = window_->Writer();
    Refresh();
}

void Console::SetLayerID(unsigned int layer_id) {
    layer_id_ = layer_id;
}

unsigned int Console::LayerID() const {
    return layer_id_;
}

void Console::NewLine() {
    cursor_column_ = 0;
    if (cursor_row_ < kRows - 1) {
        cursor_row_++;
        return;
    }

    if (window_) {
        Rectangle<int> move_src{{0, 16}, {8 * kColumns, 16 * (kRows - 1)}};
        window_->Move({0, 0}, move_src);
        // 移動して空いた最終行を塗りつぶす
        FillRectangle(*writer_, {0, 16 * (kRows - 1)}, {8 * kColumns, 16}, bg_color_);
    } else {
        // まずは塗りつぶす
        FillRectangle(*writer_, {0, 0}, {8 * kColumns, 16 * kRows}, bg_color_);
        // 表示領域を1行ずらす
        for (int row = 0; row < kRows - 1; row++) {
            memcpy(buffer_[row], buffer_[row + 1], kColumns + 1);
            WriteString(*writer_, Vector2D<int>{0, 16 * row}, buffer_[row], fg_color_);
        }
        memset(buffer_[kRows - 1], 0, kColumns + 1);
    }
}

void Console::Refresh() {
    FillRectangle(*writer_, {0, 0}, {8 * kColumns, 16 * kRows}, bg_color_);
    for (int row = 0; row < kRows; row++) {
        WriteString(*writer_, Vector2D<int>{0, 16 * row}, buffer_[row], fg_color_);
    }
}

Console* g_console;

namespace {
    char g_console_buf[sizeof(Console)];
}

void InitializeConsole() {
    g_console = new (g_console_buf) Console{
        kDesktopFGColor, kDesktopBGColor};
    g_console->SetWriter(g_screen_writer);
}
#include "mouse.hpp"

#include "graphics.hpp"
#include "layer.hpp"
#include "usb/classdriver/mouse.hpp"

namespace {
    const char g_mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
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
} // namespace

void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position) {
    for (int dy = 0; dy < kMouseCursorHeight; dy++) {
        for (int dx = 0; dx < kMouseCursorWidth; dx++) {
            if (g_mouse_cursor_shape[dy][dx] == '@') {
                pixel_writer->Write(position + Vector2D<int>{dx, dy}, {0, 0, 0});
            } else if (g_mouse_cursor_shape[dy][dx] == '.') {
                pixel_writer->Write(position + Vector2D<int>{dx, dy}, {255, 255, 255});
            } else {
                pixel_writer->Write(position + Vector2D<int>{dx, dy}, kMouseTransparentColor);
            }
        }
    }
}

Mouse::Mouse(unsigned int layer_id) : layer_id_{layer_id} {}

void Mouse::OnInterrupt(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
    const auto old_pos = position_;
    // マウスの移動範囲を画面内に制限
    auto new_pos = position_ + Vector2D<int>{displacement_x, displacement_y};
    new_pos = ElementMin(new_pos, ScreenSize() + Vector2D<int>{-1, -1});
    position_ = ElementMax(new_pos, {0, 0});

    // マウスカーソルの移動量
    const auto pos_diff = position_ - old_pos;
    g_layer_manager->Move(layer_id_, position_);

    const bool previous_left_pressed = (previous_buttons_ & 0x01);
    const bool left_pressed = (buttons & 0x01);
    if (!previous_left_pressed && left_pressed) { // 左クリック
        auto layer = g_layer_manager->FindLayerByPosition(position_, layer_id_);
        if (layer && layer->IsDraggable()) {
            drag_layer_id_ = layer->ID();
        }
    } else if (previous_left_pressed && left_pressed) { // ドラッグ中
        if (drag_layer_id_ > 0) {
            g_layer_manager->MoveRelative(drag_layer_id_, pos_diff);
        }
    } else if (previous_left_pressed && !left_pressed) { // 離した
        drag_layer_id_ = 0;
    }

    previous_buttons_ = buttons;
}

void Mouse::SetPosition(Vector2D<int> pos) {
    position_ = pos;
    g_layer_manager->Move(layer_id_, position_);
}

void InitializeMouse() {
    auto mouse_window = std::make_shared<Window>(kMouseCursorWidth, kMouseCursorHeight, g_screen_config.pixel_format);
    mouse_window->SetTransparentColor(kMouseTransparentColor);
    DrawMouseCursor(mouse_window->Writer(), {0, 0});

    auto mouse_layer_id = g_layer_manager->NewLayer()
                              .SetWindow(mouse_window)
                              .ID();

    auto mouse = std::make_shared<Mouse>(mouse_layer_id);
    mouse->SetPosition({200, 200});
    g_layer_manager->UpDown(mouse->LayerID(), std::numeric_limits<int>::max());

    // 割り込みイベント登録
    usb::HIDMouseDriver::default_observer = [mouse](uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
        mouse->OnInterrupt(buttons, displacement_x, displacement_y);
    };
}
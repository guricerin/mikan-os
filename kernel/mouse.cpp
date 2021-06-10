#include "mouse.hpp"

#include <limits>
#include <memory>

#include "graphics.hpp"
#include "layer.hpp"
#include "task.hpp"
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

    /// アクティブウィンドウにマウスイベントを送信
    void SendMouseMessage(Vector2D<int> newpos, Vector2D<int> posdiff, uint8_t buttons, uint8_t previous_buttons) {
        const auto act = g_active_layer->GetActive();
        if (!act) {
            return;
        }

        const auto layer = g_layer_manager->FindLayer(act);
        const auto task_it = g_layer_task_map->find(act);
        if (task_it == g_layer_task_map->end()) {
            return;
        }

        // ウィンドウ左上を基準とした座標に変換
        const auto relpos = newpos - layer->GetPosition();

        // 座標が変化した場合
        if (posdiff.x != 0 && posdiff.y != 0) {
            Message msg{Message::kMouseMove};
            msg.arg.mouse_move.x = relpos.x;
            msg.arg.mouse_move.y = relpos.y;
            msg.arg.mouse_move.dx = posdiff.x;
            msg.arg.mouse_move.dy = posdiff.y;
            msg.arg.mouse_move.buttons = buttons;
            g_task_manager->SendMessage(task_it->second, msg);
        }

        if (previous_buttons != buttons) {
            // 差分のあるボタンの状態はビットが立っている
            const auto diff = previous_buttons ^ buttons;
            for (int i = 0; i < 8; i++) {
                if ((diff >> i) & 1) {
                    Message msg{Message::kMouseButton};
                    msg.arg.mouse_button.x = relpos.x;
                    msg.arg.mouse_button.y = relpos.y;
                    msg.arg.mouse_button.press = (buttons >> i) & 1;
                    msg.arg.mouse_button.button = i;
                    g_task_manager->SendMessage(task_it->second, msg);
                }
            }
        }
    }
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
        if (layer && layer->IsDraggable()) { // ウィンドウクリック
            const auto y_layer = position_.y - layer->GetPosition().y;
            // タイトルバーをクリックしたとき（y座標が[0, TopLevelWindow::kTopLeftMargin.y)の範囲）にドラッグ状態にする
            if (y_layer < TopLevelWindow::kTopLeftMargin.y) {
                drag_layer_id_ = layer->ID();
            }
            g_active_layer->Activate(layer->ID());
        } else {
            g_active_layer->Activate(0); // すべてのウィンドウを非アクティブ化
        }
    } else if (previous_left_pressed && left_pressed) { // ドラッグ中
        if (drag_layer_id_ > 0) {
            g_layer_manager->MoveRelative(drag_layer_id_, pos_diff);
        }
    } else if (previous_left_pressed && !left_pressed) { // 離した
        drag_layer_id_ = 0;
    }

    // ウィンドウをドラッグ中（ウィンドウ内でのマウス座標は変化していない）の場合はメッセージを送信しない
    if (drag_layer_id_ == 0) {
        SendMouseMessage(new_pos, pos_diff, buttons, previous_buttons_);
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

    g_active_layer->SetMouseLayer(mouse_layer_id);
}
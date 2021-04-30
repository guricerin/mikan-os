#include "terminal.hpp"

#include "font.hpp"
#include "layer.hpp"
#include "logger.hpp"

Terminal::Terminal() {
    window_ = std::make_shared<TopLevelWindow>(
        kColumns * 8 + 8 + TopLevelWindow::kMarginX,
        kRows * 16 + 8 + TopLevelWindow::kMarginY,
        g_screen_config.pixel_format,
        "MikanTerm");
    DrawTerminal(*window_->InnerWriter(), {0, 0}, window_->InnerSize());

    layer_id_ = g_layer_manager->NewLayer()
                    .SetWindow(window_)
                    .SetDraggable(true)
                    .ID();
}

Rectangle<int> Terminal::BlinkCursor() {
    cursol_visible_ = !cursol_visible_;
    DrawCursor(cursol_visible_);

    return {CalcCursorPos(), {7, 15}};
}

void Terminal::DrawCursor(bool visible) {
    const auto color = visible ? ToColor(0xffffff) : ToColor(0);
    FillRectangle(*window_->Writer(), CalcCursorPos(), {7, 15}, color);
}

Rectangle<int> Terminal::InputKey(uint8_t modifier, uint8_t keycode, char ascii) {
    DrawCursor(false);

    Rectangle<int> draw_area{CalcCursorPos(), {8 * 2, 16}};

    if (ascii == '\n') {
        linebuf_[linebuf_index_] = 0; // null文字
        linebuf_index_ = 0;
        cursor_.x = 0;
        Log(kWarn, "line: %s\n", &linebuf_[0]);
        if (cursor_.y < kRows - 1) {
            cursor_.y++;
        } else {
            // 最終行ならスクロール
            Scroll1();
        }
        draw_area.pos = TopLevelWindow::kTopLeftMargin;
        draw_area.size = window_->InnerSize();
    } else if (ascii == '\b') {
        if (cursor_.x > 0) {
            // 1文字消してカーソルを左に戻す
            cursor_.x--;
            FillRectangle(*window_->Writer(), CalcCursorPos(), {8, 16}, {0, 0, 0});
            draw_area.pos = CalcCursorPos();

            if (linebuf_index_ > 0) {
                linebuf_index_--;
            }
        }
    } else if (ascii != 0) {
        if (cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
            linebuf_[linebuf_index_] = ascii;
            linebuf_index_++;
            WriteAscii(*window_->Writer(), CalcCursorPos(), ascii, {255, 255, 255});
            cursor_.x++;
        }
    }

    DrawCursor(true);
    return draw_area;
}

Vector2D<int> Terminal::CalcCursorPos() const {
    return TopLevelWindow::kTopLeftMargin + Vector2D<int>{4 + 8 * cursor_.x, 4 + 16 * cursor_.y};
}

void Terminal::Scroll1() {
    Rectangle<int> move_src{
        TopLevelWindow::kTopLeftMargin + Vector2D<int>{4, 4 + 16},
        {8 * kColumns, 16 * (kRows - 1)}};
    window_->Move(TopLevelWindow::kTopLeftMargin + Vector2D<int>{4, 4}, move_src);
    // 最終行を塗りつぶす
    FillRectangle(*window_->InnerWriter(), {4, 4 + 16 * cursor_.y}, {8 * kColumns, 16}, {0, 0, 0});
}

void TaskTerminal(uint64_t task_id, int64_t data) {
    // グローバル変数を扱う際は割り込みを禁止しておくのが無難
    __asm__("cli");
    Task& task = g_task_manager->CurrentTask();
    Terminal* terminal = new Terminal;
    g_layer_manager->Move(terminal->LayerID(), {100, 200});
    g_active_layer->Activate(terminal->LayerID());
    g_layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
    __asm__("sti");

    while (true) {
        __asm__("cli");
        auto msg = task.ReceiveMessage();
        if (!msg) {
            task.Sleep();
            __asm__("sti");
            continue;
        }

        switch (msg->type) {
        case Message::kTimerTimeout: {
            // 一定時間ごとにカーゾルを点滅させる
            const auto area = terminal->BlinkCursor();
            Message msg = MakeLayerMessage(task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
            // メインタスクに描画処理を要求
            __asm__("cli");
            g_task_manager->SendMessage(1, msg);
            __asm__("sti");
        } break;
        case Message::kKeyPush: {
            const auto area = terminal->InputKey(msg->arg.keyboard.modifier,
                                                 msg->arg.keyboard.keycode,
                                                 msg->arg.keyboard.ascii);
            Message msg = MakeLayerMessage(task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
            // メインタスクに描画処理を要求
            __asm__("cli");
            g_task_manager->SendMessage(1, msg);
            __asm__("sti");
        } break;
        default:
            break;
        }
    }
}
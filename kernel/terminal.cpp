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

/// カーソルの点滅を切り替える
void Terminal::BlinkCursor() {
    cursol_visible_ = !cursol_visible_;
    DrawCursor(cursol_visible_);
}

void Terminal::DrawCursor(bool visible) {
    const auto color = visible ? ToColor(0xffffff) : ToColor(0);
    const auto pos = Vector2D<int>{4 + 8 * cursor_.x, 5 + 16 * cursor_.y};
    FillRectangle(*window_->InnerWriter(), pos, {7, 15}, color);
}

void TaskTerminal(uint64_t task_id, int64_t data) {
    // グローバル変数を扱う際は割り込みを禁止しておくのが無難
    __asm__("cli");
    Task& task = g_task_manager->CurrentTask();
    Terminal* terminal = new Terminal;
    g_layer_manager->Move(terminal->LayerID(), {100, 200});
    g_active_layer->Activate(terminal->LayerID());
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
        case Message::kTimerTimeout:
            // 一定時間ごとにカーゾルを点滅させる
            terminal->BlinkCursor();
            // メインタスクに描画処理を要求
            {
                Message msg{Message::kLayer, task_id};
                msg.arg.layer.layer_id = terminal->LayerID();
                msg.arg.layer.op = LayerOperation::Draw;
                __asm__("cli");
                g_task_manager->SendMessage(1, msg);
                __asm__("sti");
            }
            break;
        default:
            break;
        }
    }
}
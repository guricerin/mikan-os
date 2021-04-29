/// ターミナルウィンドウ

#pragma once

#include "layer.hpp"
#include "task.hpp"
#include "window.hpp"
#include <deque>
#include <map>

class Terminal {
public:
    static const int kRows = 15, kColumns = 60;

    Terminal();
    unsigned int LayerID() const { return layer_id_; }
    /// カーソルの点滅を切り替え、カーソルの描画領域を返す
    Rectangle<int> BlinkCursor();

private:
    std::shared_ptr<TopLevelWindow> window_;
    unsigned int layer_id_;
    /// カーソルの位置
    Vector2D<int> cursor_{0, 0};
    bool cursol_visible_{false};

    void DrawCursor(bool visible);
};

void TaskTerminal(uint64_t task_id, int64_t data);
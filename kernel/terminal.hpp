/// ターミナルウィンドウ

#pragma once

#include <deque>
#include <map>

#include "layer.hpp"
#include "task.hpp"
#include "window.hpp"

class Terminal {
public:
    static const int kRows = 15, kColumns = 60;
    static const int kLineMax = 128;

    Terminal();
    unsigned int LayerID() const { return layer_id_; }
    /// カーソルの点滅を切り替え、カーソルの描画領域を返す
    Rectangle<int> BlinkCursor();
    // キー入力を受付け、再描画すべき範囲を返す
    Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);

private:
    std::shared_ptr<TopLevelWindow> window_;
    unsigned int layer_id_;
    /// カーソルの現在位置
    Vector2D<int> cursor_{0, 0};
    bool cursol_visible_{false};
    int linebuf_index_{0};
    /// キー入力を1行分ためておくバッファ
    std::array<char, kLineMax> linebuf_{};

    void DrawCursor(bool visible);
    Vector2D<int> CalcCursorPos() const;
    /// 1行だけスクロール
    void Scroll1();
};

void TaskTerminal(uint64_t task_id, int64_t data);
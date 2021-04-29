#pragma once

#include <cstdint>

enum class LayerOperation {
    /// 指定の座標に移動
    Move,
    /// 移動量を指定
    MoveRelative,
    /// 全体描画
    Draw,
    /// 描画領域を指定
    DrawArea,
};

/// 割り込みメッセージ
struct Message {
    enum Type {
        kInterruptXHCI,
        kTimerTimeout,
        kKeyPush,
        kLayer,
        kLayerFinish,
    } type;

    /// メッセージ送信元のタスクID
    uint64_t src_task;

    union {
        /// タイマーイベント
        struct {
            unsigned long timeout;
            int value;
        } timer;

        /// キー押下イベント
        struct {
            uint8_t modifier;
            uint8_t keycode;
            char ascii;
        } keyboard;

        /// 描画イベント
        struct {
            LayerOperation op;
            /// 操作対象のレイヤーID
            unsigned int layer_id;
            int x, y;
            int w, h;
        } layer;
    } arg;
};

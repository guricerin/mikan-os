#pragma once

#include <cstdint>

enum class LayerOperation {
    Move,
    MoveRelative,
    Draw,
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
        } layer;
    } arg;
};

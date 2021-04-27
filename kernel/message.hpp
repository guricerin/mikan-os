#pragma once

#include <cstdint>

/// 割り込みメッセージ
struct Message {
    enum Type {
        kInterruptXHCI,
        kTimerTimeout,
        kKeyPush,
    } type;

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
    } arg;
};

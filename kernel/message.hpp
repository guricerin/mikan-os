#pragma once

/// 割り込みメッセージ
struct Message {
    enum Type {
        kInterruptXHCI,
        kTimerTimeout,
    } type;

    union {
        struct {
            unsigned long timeout;
            int value;
        } timer;
    } arg;
};

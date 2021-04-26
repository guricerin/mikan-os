#pragma once

/// 割り込みメッセージ
struct Message {
    enum Type {
        kInterruptXHCI,
    } type;
};

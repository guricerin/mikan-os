#pragma once

/// セグメントディスクリプタと割り込みディスクリプタのための共通定義
enum class DescriptorType {
    kUpper8Bytes = 0,
    kLDT = 2,
    kTSSAvailable = 9,
    kTSSBusy = 11,
    kCallGate = 12,
    // 通常の割り込み
    kInterruptGate = 14,
    kTrapGate = 15,

    // data segment types
    kReadWrite = 2,
    // code segment type
    kExecuteRead = 10,
};
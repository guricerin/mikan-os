/// セグメンテーション
/// 元はメモリを区画分けする機能だったが、x86-64 64bitモードではページングにお株を奪われ、事実上無効化されている
/// もっぱらCPUの動作権限を決定する機能

#pragma once

#include <array>
#include <cstdint>

#include "x86_descriptor.hpp"

/// 8byte
union SegmentDescriptor {
    uint64_t data;
    struct {
        /// limit : 区画のサイズ
        /// base : セグメントの開始アドレス
        /// 64bitモードでは、上記2つはそもそも無視される
        uint64_t limit_low : 16;
        uint64_t base_low : 16;
        uint64_t base_middle : 8;
        DescriptorType type : 4;
        /// 1 : コードセグメント or データセグメント
        uint64_t system_segment : 1;
        /// DPL（権限レベル）
        /// 0 : 特権モード、カーネルモード
        /// 3 : ユーザーモード
        uint64_t descriptor_privilege_level : 2;
        /// 1 : ディスクリプタが有効
        uint64_t present : 1;
        uint64_t limit_high : 4;
        /// OSが自由に使用していいビット（今回は特に使用しない）
        uint64_t available : 1;
        /// 1 : 64bitモード用のコードセグメント
        /// 0 : 互換モード（32bit, 16bit）用のコードセグメント
        uint64_t long_mode : 1;
        /// long_modeが1なら必ず0にする
        uint64_t default_operation_size : 1;
        /// 1 : リミットを4KiB単位として解釈
        uint64_t granularity : 1;
        uint64_t base_high : 8;
    } __attribute__((packed)) bits;
} __attribute__((packed));

/// コードセグメント : 実行可能なセグメント
void SetCodeSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit);

/// データセグメント : 実行不可能なセグメント
void SetDataSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit);

/// それぞれ、GDTのどのインデックスに配置されるかを表す
const uint16_t kKernelCS = 1 << 3;
const uint16_t kKernelSS = 2 << 3;
const uint16_t kKernelDS = 0;
const uint16_t kTSS = 5 << 3; // GDT[5]

void SetupSegments();
void InitializeSegmentation();
/// TSSを初期化してGDTに設定する
void InitializeTSS();
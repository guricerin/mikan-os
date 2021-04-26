/// ACPI : Advanced Configuration and Power Interface)
/// コンピュータの構成や電源を管理するための規格

/// ACPI PMタイマ : Power Management
/// 電源管理のためのタイマ。他の用途に使用してもよい
/// Local APICとはちがって、動作周波数がわかっている

/// FADT
/// ACPI PMタイマを使う際に必要な、タイマを制御するレジスタ群のポート番号を記載しているテーブル

/// XSDT : Extended System Descriptor Table
/// FADTのメモリアドレスを記載しているテーブル

/// RSDP : Root System Descriptor Pointer
/// XSDTのメモリアドレスを記載している構造体

#pragma once

#include <cstdint>

namespace acpi {
    struct RSDP {
        /// RSDPのシグネチャ。「RSD__PTR__」の8文字
        char signature[8];
        /// 前半20byteのためのチェックサム
        uint8_t checksum;
        /// OEMの名前
        char oem_id[6];
        /// RSDP構造体のバージョン番号
        uint8_t revision;
        /// RSDTを指す32bitのアドレス
        uint32_t rsdt_address;
        /// RSDP全体のバイト数
        uint32_t length;
        /// XSDTを指す64bitの物理アドレス
        uint64_t xsdt_address;
        /// 拡張領域を含めたRSDP全体のチェックサム
        uint8_t extended_checksum;
        /// 予約領域
        char reserved[3];

        bool IsValid() const;
    } __attribute__((packed));

    void Initialize(const RSDP& rsdp);
} // namespace acpi
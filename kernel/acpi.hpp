/// ACPI : Advanced Configuration and Power Interface)
/// コンピュータの構成や電源を管理するための規格

/// ACPI PMタイマ : Power Management
/// 電源管理のためのタイマ。他の用途に使用してもよい
/// Local APICとはちがって、動作周波数がわかっている

#pragma once

#include <cstddef>
#include <cstdint>

namespace acpi {
    /// RSDP : Root System Descriptor Pointer
    /// XSDTのメモリアドレスを記載している構造体
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

    /// XSDT自身やXSDTから指される他のデータ構造に共通する記述ヘッダ
    struct DescriptionHeader {
        char signature[4];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        char oem_id[6];
        char oem_table_id[8];
        uint32_t oem_revision;
        uint32_t creator_id;
        uint32_t creator_revision;

        bool IsValid(const char* expected_signature) const;
    } __attribute__((packed));

    /// XSDT : Extended System Descriptor Table
    /// FADTのメモリアドレスを記載しているテーブル
    struct XSDT {
        DescriptionHeader header;

        const DescriptionHeader& operator[](size_t i) const;
        /// XSDTが保持するデータ構造のアドレスの個数
        size_t Count() const;
    } __attribute__((packed));

    /// FADT
    /// ACPI PMタイマを使う際に必要な、タイマを制御するレジスタ群のポート番号を記載しているテーブル
    struct FADT {
        DescriptionHeader header;

        char reserved[76 - sizeof(header)];
        uint32_t pm_tmr_blk;
        char reserved2[112 - 80];
        uint32_t flags;
        char reserved3[276 - 116];
    } __attribute__((packed));

    extern const FADT* g_fadt;
    /// ACPI PMタイマの周波数 : 3.579545MHz
    /// 24ビットカウンタなら約4.7秒で1周して0になる
    const int kPMTimerFreq = 3579545;

    void Initialize(const RSDP& rsdp);
    /// 指定したミリ秒が経過するのを待機
    void WaitMillisecondes(unsigned long msec);
} // namespace acpi
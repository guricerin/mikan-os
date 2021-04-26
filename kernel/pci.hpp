/// PCI : Peripheral Component Interconnect
/// マザーボードと周辺機器を繋ぐための規格

#pragma once

#include <array>
#include <cstdint>

#include "error.hpp"

namespace pci {
    /// CONFIG_ADDRESSレジスタのIOポートアドレス
    const uint16_t kConfigAddress = 0x0cf8;
    /// CONFIG_DATAレジスタのIOポートアドレス
    const uint16_t kConfigData = 0x0cfc;

    /// デバイスのクラスコード
    struct ClassCode {
        uint8_t base, sub, interface;

        /// ベースクラスが等しい場合に真を返す
        bool Match(uint8_t b) { return b == base; }
        /// ベースクラスとサブクラスが等しい場合に真を返す
        bool Match(uint8_t b, uint8_t s) { return Match(b) && s == sub; }
        /// ベース，サブ，インターフェースが等しい場合に真を返す
        bool Match(uint8_t b, uint8_t s, uint8_t i) {
            return Match(b, s) && i == interface;
        }
    };

    /// PCIデバイスを操作するための基礎データを格納する
    /// バス番号，デバイス番号，ファンクション番号はデバイスを特定するのに必須．
    /// その他の情報は単に利便性のために加えてある．
    struct Device {
        uint8_t bus, device, function, header_type;
        ClassCode class_code;
    };

    /// ScanAllBus()により発見されたPCIデバイスの一覧
    /// 1つのPCIバスに最大32個のPCIデバイスが接続される
    inline std::array<Device, 32> g_devices;
    /// devicesの有効な要素の数
    inline int g_num_device;

    /// CONFIG_ADDRESSに指定の整数を書き込む
    void WriteAddress(uint32_t address);
    /// CONFIG_DATAに指定の整数を書き込む
    void WriteData(uint32_t value);
    /// CONFIG_DATAから32bit整数を読み込む
    uint32_t ReadData();

    /// ベンダIDレジスタを読み込む（全ヘッダタイプ共通）
    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);
    inline uint16_t ReadVendorId(const Device& dev) {
        return ReadVendorId(dev.bus, dev.device, dev.function);
    }
    /// デバイスIDレジスタを読み込む（全ヘッダタイプ共通）
    uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function);
    /// ヘッダタイプレジスタを読み込む（全ヘッダタイプ共通）
    uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function);

    /// クラスコードレジスタを読み込む（全ヘッダタイプ共通）
    /// 返される32bit整数の構造は次の通り．
    ///   - 31:24 : ベースクラス
    ///   - 23:16 : サブクラス
    ///   - 15:8  : インターフェース
    ///   - 7:0   : リビジョン
    ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

    /// 指定された PCI デバイスの 32 ビットレジスタを読み取る */
    uint32_t ReadConfReg(const Device& device, uint8_t reg_addr);
    void WriteConfReg(const Device& device, uint8_t reg_addr, uint32_t value);

    /// バス番号レジスタを読み込む（ヘッダタイプ1用）
    /// 返される32bit整数の構造は次の通り．
    ///   - 23:16 : サブオーディネイトバス番号
    ///   - 15:8  : セカンダリバス番号
    ///   - 7:0   : リビジョン番号
    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);

    /// 単一ファンクションの場合に真を返す．
    bool IsSingleFunctionDevice(uint8_t header_type);

    /// PCIデバイスをすべて探索しdevicesに格納する
    /// バス0から再帰的にPCIデバイスを探索し，devicesの先頭から詰めて書き込む．
    /// 発見したデバイスの数をnum_devicesに設定する．
    Error ScanAllBus();

    constexpr uint8_t CalcBarAddress(unsigned int bar_index) {
        return 0x10 + 4 * bar_index;
    }

    WithError<uint64_t> ReadBar(Device& device, unsigned int bar_index);

    /// MSI : MessageSignaled Interrupt
    /// PCI規格で定められた割り込み方式
    /// 特定のメモリアドレスへの書き込みにより割り込みをCPUに通知
    /// MSI-XはMSIの拡張規格

    /// PCIケイパビリティレジスタの共通ヘッダ
    union CapabilityHeader {
        uint32_t data;
        struct {
            uint32_t cap_id : 8;
            uint32_t next_ptr : 8;
            uint32_t cap : 16;
        } __attribute__((packed)) bits;
    } __attribute__((packed));

    const uint8_t kCapabilityMSI = 0x05;
    const uint8_t kCapabilityMSIX = 0x11;

    /// 指定されたPCIデバイスの指定されたケイパビリティレジスタを読み込む
    /// device : PCIデバイス
    /// ケイパビリティレジスタのコンフィグレーション空間アドレス
    CapabilityHeader ReadCapabilityHeader(const Device& device, uint8_t addr);

    /// MSIケイパビリティ構造
    /// 64bitサポートの有無などで亜種が多い。
    /// この構造体は各亜種に対応するために最大の亜種に合わせてメンバを定義してある。
    struct MSICapability {
        union {
            uint32_t data;
            struct {
                uint32_t cap_id : 8;
                uint32_t next_ptr : 8;
                uint32_t msi_enable : 1;
                uint32_t multi_msg_capable : 3;
                uint32_t multi_msg_enable : 3;
                uint32_t addr_64_capable : 1;
                uint32_t per_vector_mask_capable : 1;
                uint32_t : 7;
            } __attribute__((packed)) bits;
        } __attribute__((packed)) header;

        uint32_t msg_addr;
        uint32_t msg_upper_addr;
        uint32_t msg_data;
        uint32_t mask_bits;
        uint32_t pending_bits;
    } __attribute__((packed));

    /// MSI or MSI-X割り込みを設定
    /// device : 設定対象のPCIデバイス
    /// msg_addr : 割り込み発生時にメッセージを書き込むアドレス
    /// msg_data : 割り込み発生時に書き込むメッセージの値
    /// num_vector_exponent : 割り当てるベクタ数（2^n の n を指定）
    Error ConfigureMSI(const Device& device, uint32_t msg_addr, uint32_t msg_data, unsigned int num_vector_exponent);

    enum class MSITriggerMode {
        kEdge = 0,
        kLevel = 1,
    };

    enum class MSIDeliveryMode {
        kFixed = 0b000,
        kLowestPriprity = 0b001,
        kSMI = 0b010,
        kNMI = 0b100,
        kINIT = 0b101,
        kExtINT = 0b111,
    };

    Error ConfigureMSIFixedDestination(
        const Device& device, uint8_t apic_id,
        MSITriggerMode trigger_mode, MSIDeliveryMode delivery_mode,
        uint8_t vector, unsigned int num_vector_exponent);
} // namespace pci

void InitializePCI();
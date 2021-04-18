/** 
 * pciバス制御
 */

#pragma once

#include <array>
#include <cstdint>

#include "error.hpp"

namespace pci {
    /// CONFIG_ADDRESSレジスタのIOポートアドレス
    const uint16_t CONFIG_ADDRESS = 0x0cf8;
    /// CONFIG_DATAレジスタのIOポートアドレス
    const uint16_t CONFIG_DATA = 0x0cfc;

    /// CONFIG_ADDRESSに指定の整数を書き込む
    void WriteAddress(uint32_t address);
    /// CONFIG_DATAに指定の整数を書き込む
    void WriteData(uint32_t value);
    /// CONFIG_DATAから32bit整数を読み込む
    uint32_t ReadData();

    /// ベンダIDレジスタを読み込む（全ヘッダタイプ共通）
    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);
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
    uint32_t ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

    /// バス番号レジスタを読み込む（ヘッダタイプ1用）
    /// 返される32bit整数の構造は次の通り．
    ///   - 23:16 : サブオーディネイトバス番号
    ///   - 15:8  : セカンダリバス番号
    ///   - 7:0   : リビジョン番号
    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);

    /// 単一ファンクションの場合に真を返す．
    bool IsSingleFunctionDevice(uint8_t header_type);

    /// PCIデバイスを操作するための基礎データを格納する
    /// バス番号，デバイス番号，ファンクション番号はデバイスを特定するのに必須．
    /// その他の情報は単に利便性のために加えてある．
    struct Device {
        uint8_t bus, device, function, header_type;
    };
    /// ScanAllBus()により発見されたPCIデバイスの一覧
    /// 1つのPCIバスに最大32個のPCIデバイスが接続される
    inline std::array<Device, 32> g_devices;
    /// devicesの有効な要素の数
    inline int g_num_device;
    /// PCIデバイスをすべて探索しdevicesに格納する
    /// バス0から再帰的にPCIデバイスを探索し，devicesの先頭から詰めて書き込む．
    /// 発見したデバイスの数をnum_devicesに設定する．
    Error ScanAllBus();
} // namespace pci
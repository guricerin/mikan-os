#include "pci.hpp"

#include "asmfunc.h"

namespace {
    using namespace pci;

    /// CONFIG_ADDRESS用の32bit整数を生成
    uint32_t MakeAddress(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_addr) {
        auto shl = [](uint32_t x, unsigned int bits) {
            return x << bits;
        };

        return shl(1, 31) // enable bit
               | shl(bus, 16) | shl(device, 11) | shl(function, 8) | (reg_addr & 0xfcu);
    }

    /// devices[num_device]に情報を書き込みnum_deviceをインクリ
    Error AddDevice(uint8_t bus, uint8_t device, uint8_t function, uint8_t header_type) {
        if (g_num_device == g_devices.size()) {
            return ErrorCode::Full;
        }

        g_devices[g_num_device] = Device{bus, device, function, header_type};
        g_num_device++;
        return ErrorCode::Success;
    }

    Error ScanBus(uint8_t bus);

    /// 指定のファンクションをdevicesに追加
    /// もしPCI-PCIブリッジなら、セカンダリバスに対しScanBuxを実行
    Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function) {
        auto header_type = ReadHeaderType(bus, device, function);
        if (auto err = AddDevice(bus, device, function, header_type)) {
            return err;
        }

        auto class_code = ReadClassCode(bus, device, function);
        uint8_t base = (class_code >> 24) & 0xffu;
        uint8_t sub = (class_code >> 16) & 0xffu;

        if (base == 0x06u && sub == 0x04u) {
            // standard PCI-PCI bridge
            auto bus_numbers = ReadBusNumbers(bus, device, function);
            uint8_t secondary_bus = (bus_numbers >> 8) & 0xffu;
            return ScanBus(secondary_bus);
        }

        return ErrorCode::Success;
    }

    /// 指定のデバイス番号の各ファンクションをスキャン
    /// 有効なファンクションを見つけたらScanFunctionを実行
    Error ScanDevice(uint8_t bus, uint8_t device) {
        if (auto err = ScanFunction(bus, device, 0)) {
            return err;
        }
        if (IsSingleFunctionDevice(ReadHeaderType(bus, device, 0))) {
            return ErrorCode::Success;
        }

        for (uint8_t function = 1; function < 8; function++) {
            // 無効なベンダIDは無視
            if (ReadVendorId(bus, device, function) == 0xffffu) {
                continue;
            }
            if (auto err = ScanFunction(bus, device, function)) {
                return err;
            }
        }

        return ErrorCode::Success;
    }

    /// 指定のバス番号の各デバイスをスキャン
    /// 有効なデバイスを見つけたらScanDeviceを実行
    Error ScanBus(uint8_t bus) {
        for (uint8_t device = 0; device < 32; device++) {
            if (ReadVendorId(bus, device, 0) == 0xffffu) {
                continue;
            }
            if (auto err = ScanDevice(bus, device)) {
                return err;
            }
        }

        return ErrorCode::Success;
    }
} // namespace

namespace pci {
    void WriteAddress(uint32_t address) {
        IoOut32(CONFIG_ADDRESS, address);
    }
} // namespace pci
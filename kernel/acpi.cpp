#include "acpi.hpp"

#include <cstdlib>
#include <cstring>

#include "asmfunc.h"
#include "logger.hpp"

namespace {
    /// dataの前半bytes分のメモリ領域をバイト単位で総和
    template <typename T>
    uint8_t SumBytes(const T* data, size_t bytes) {
        return SumBytes(reinterpret_cast<const uint8_t*>(data), bytes);
    }

    /// テンプレートの特殊化（この場合はuint8_tの場合に、上ではなくこちらの関数がコールされる）
    template <>
    uint8_t SumBytes<uint8_t>(const uint8_t* data, size_t bytes) {
        uint8_t sum = 0;
        for (size_t i = 0; i < bytes; i++) {
            sum += data[i];
        }
        return sum;
    }
} // namespace

namespace acpi {
    bool RSDP::IsValid() const {
        // signatureはヌル文字で終端されていないので文字数を指定
        if (strncmp(this->signature, "RSD PTR ", 8) != 0) {
            Log(kDebug, "invalide signature: %,8s\n", this->signature);
            return false;
        }
        if (this->revision != 2) {
            Log(kDebug, "ACPI revision must be 2: %d\n", this->revision);
            return false;
        }
        // チェックサムによる誤り検出
        if (auto sum = SumBytes(this, 20); sum != 0) {
            Log(kDebug, "sum of 20 bytes must be 0: %d\n", sum);
            return false;
        }
        // チェックサムによる誤り検出
        if (auto sum = SumBytes(this, 36); sum != 0) {
            Log(kDebug, "sum of 36 bytes must be 0: %d\n", sum);
            return false;
        }
        return true;
    }

    bool DescriptionHeader::IsValid(const char* expected_signature) const {
        if (strncmp(this->signature, expected_signature, 4) != 0) {
            Log(kDebug, "invalid signature: %.4\n", this->signature);
            return false;
        }
        if (auto sum = SumBytes(this, this->length); sum != 0) {
            Log(kDebug, "sum of %u bytes must be 0: %d\n", this->length, sum);
            return false;
        }
        return true;
    }

    const DescriptionHeader& XSDT::operator[](size_t i) const {
        // XSDTのDescriptor Headerの後ろは、各データ構造が連続したメモリ領域に並んでいる
        auto entries = reinterpret_cast<const uint64_t*>(&this->header + 1);
        // 各データ構造の先頭もDescriptor Headerなので、先頭のアドレスをDescriptor Header&にキャストする
        return *reinterpret_cast<const DescriptionHeader*>(entries[i]);
    }

    size_t XSDT::Count() const {
        return (this->header.length - sizeof(DescriptionHeader)) / sizeof(uint64_t);
    }

    const FADT* g_fadt;

    void Initialize(const RSDP& rsdp) {
        if (!rsdp.IsValid()) {
            Log(kError, "RSDP is not valid\n");
            exit(1);
        }

        const XSDT& xsdt = *reinterpret_cast<const XSDT*>(rsdp.xsdt_address);
        if (!xsdt.header.IsValid("XSDT")) {
            Log(kError, "XSDT is not valid\n");
            exit(1);
        }

        // XSDTが持つアドレス配列からFADTを検索
        g_fadt = nullptr;
        for (int i = 0; i < xsdt.Count(); i++) {
            const auto& entry = xsdt[i];
            if (entry.IsValid("FACP")) { // FACP is the signature of FADT
                g_fadt = reinterpret_cast<const FADT*>(&entry);
                break;
            }
        }

        if (g_fadt == nullptr) {
            Log(kError, "FADT is not found\n");
            exit(1);
        }
    }

    void WaitMillisecondes(unsigned long msec) {
        // PMタイマのビット幅が32bit : true, 24bit : false
        const bool pm_timer_32 = (g_fadt->flags >> 8) & 1;
        // 現在のPMタイマのカウント値
        const uint32_t start = IoIn32(g_fadt->pm_tmr_blk);
        uint32_t end = start + kPMTimerFreq * msec / 1000;
        if (!pm_timer_32) {
            end &= 0x00ffffffu;
        }

        if (end < start) { // endのoverflow対策
            while (IoIn32(g_fadt->pm_tmr_blk) >= start)
                ;
        }
        while (IoIn32(g_fadt->pm_tmr_blk) < end)
            ;
    }
} // namespace acpi
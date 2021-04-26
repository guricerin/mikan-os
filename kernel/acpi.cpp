#include "acpi.hpp"

#include <cstdlib>
#include <cstring>

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

    void Initialize(const RSDP& rsdp) {
        if (!rsdp.IsValid()) {
            Log(kError, "RSDP is not valid\n");
            exit(1);
        }
    }
} // namespace acpi
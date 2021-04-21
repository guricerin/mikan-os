#pragma once

#include <stdint.h>

/// メインメモリのどの領域がどんな用途で使われているか
struct MemoryMap {
    unsigned long long buffer_size;
    void* buffer;
    unsigned long long map_size;        // メモリマップ全体のバイト数
    unsigned long long map_key;         // 値が変化したらメモリマップが変化していることを示す
    unsigned long long descriptor_size; // メモリディスクリプタ（メモリマップの各要素）のバイト数
    uint32_t descriptor_version;        // 未使用
};

struct MemoryDescriptor {
    uint32_t type;
    uintptr_t physical_start;
    uintptr_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

#ifdef __cplusplus
enum class MemoryType {
    kEfiReservedMemoryType,
    kEfiLoaderCode,
    kEfiLoaderData,
    kEfiBootServiceCode,
    kEfiBootServiceData,
    kEfiRuntimeSericeCode,
    kEfiRuntimeSericeData,
    kEfiConventionalMemory,
    kEfiUnusableMemory,
    kEfiACPIReclaimMemory,
    kEfiACPIMemoryNVS,
    kEfiMemoryMappedIO,
    kEfiMemoryMappedIOPortSpace,
    kEfiPalCode,
    kEfiPersistentMemory,
    kEfiMaxMemoryType,
};

inline bool operator==(uint32_t lhs, MemoryType rhs) {
    return lhs == static_cast<uint32_t>(rhs);
}

inline bool operator==(MemoryType lhs, uint32_t rhs) {
    return rhs == lhs;
}

/// 空き領域があるか判定
inline bool IsAvailable(MemoryType type) {
    // UEFI Boot Service を抜けた後にOSが自由に使用していい領域
    return type == MemoryType::kEfiBootServiceCode ||
           type == MemoryType::kEfiBootServiceData ||
           type == MemoryType::kEfiConventionalMemory;
}

const int kUEFIPageSize = 4096;
#endif
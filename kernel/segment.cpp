#include "segment.hpp"

#include "asmfunc.h"

namespace {
    /// Global Discriptor Table
    std::array<SegmentDescriptor, 3> g_gdt;
} // namespace

void SetCodeSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit) {
    desc.data = 0;

    desc.bits.base_low = base & 0xffffu;
    desc.bits.base_middle = (base >> 16) & 0xffu;
    desc.bits.base_high = (base >> 24) & 0xffu;

    desc.bits.limit_low = limit & 0xffffu;
    desc.bits.limit_high = (limit >> 16) & 0xffu;

    desc.bits.type = type;
    // 1: code & data segment
    desc.bits.system_segment = 1;
    desc.bits.descriptor_privilege_level = descriptor_privilege_level;
    desc.bits.present = 1;
    desc.bits.available = 0;
    desc.bits.long_mode = 1;
    // should be 0 when long_mode == 1
    desc.bits.default_operation_size = 0;
    desc.bits.granularity = 1;
}

void SetDataSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit) {
    SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
    desc.bits.long_mode = 0;
    // 32bit stack segment
    desc.bits.default_operation_size = 1;
}

/// セグメントの設定（GDTを再構築）
void SetupSegments() {
    // null descriptor（GDTの0番目は使用されない）
    g_gdt[0].data = 0;
    SetCodeSegment(g_gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xfffff);
    SetDataSegment(g_gdt[2], DescriptorType::kReadWrite, 0, 0, 0xfffff);
    // g_gdtを正式なGDTとしてCPUに登録
    LoadGDT(sizeof(g_gdt) - 1, reinterpret_cast<uintptr_t>(&g_gdt[0]));
}

void InitializeSegmentation() {
    SetupSegments();

    // ヌルディスクリプタにする
    SetDSAll(kKernelDS);
    SetCSSS(kKernelCS, kKernelSS);
}
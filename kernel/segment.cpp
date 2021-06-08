#include "segment.hpp"

#include "asmfunc.h"
#include "interrupt.hpp"
#include "logger.hpp"
#include "memory_manager.hpp"

namespace {
    /// GDT : Global Discriptor Table
    std::array<SegmentDescriptor, 7> g_gdt;
    /// TSS : Task-State Segment
    /// TSS用のセグメントはGDTの2要素分を消費する
    std::array<uint32_t, 26> g_tss;

    static_assert((kTSS >> 3) + 1 < g_gdt.size());
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

/// system segmentビットが0であるセグメントを設定
void SetSystemSegment(SegmentDescriptor& desc,
                      DescriptorType type,
                      unsigned int descriptor_privilege_level,
                      uint32_t base,
                      uint32_t limit) {
    SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
    desc.bits.system_segment = 0;
    desc.bits.long_mode = 0;
}

/// セグメントの設定（GDTを再構築）
void SetupSegments() {
    // null descriptor（GDTの0番目は使用されない）
    g_gdt[0].data = 0;
    // カーネル用のセグメントディスクリプタ
    SetCodeSegment(g_gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xfffff);
    SetDataSegment(g_gdt[2], DescriptorType::kReadWrite, 0, 0, 0xfffff);
    // アプリ用のセグメントディスクリプタ
    // 権限レベルを最低にする
    SetDataSegment(g_gdt[3], DescriptorType::kReadWrite, 3, 0, 0xfffff);
    SetCodeSegment(g_gdt[4], DescriptorType::kExecuteRead, 3, 0, 0xfffff);
    // g_gdtを正式なGDTとしてCPUに登録
    LoadGDT(sizeof(g_gdt) - 1, reinterpret_cast<uintptr_t>(&g_gdt[0]));
}

void InitializeSegmentation() {
    SetupSegments();

    // ヌルディスクリプタにする
    SetDSAll(kKernelDS);
    SetCSSS(kKernelCS, kKernelSS);
}

void InitializeTSS() {
    const int kRSP0Frames = 8;
    auto [stack0, err] = g_memory_manager->Allocate(kRSP0Frames);
    if (err) {
        Log(kError, "failed to allocate rsp0: %s\n", err.Name());
        exit(1);
    }

    uint64_t rsp0 = reinterpret_cast<uint64_t>(stack0.Frame()) + kRSP0Frames * 4096;
    g_tss[1] = rsp0 & 0xffffffff;
    g_tss[2] = rsp0 >> 32;

    uint64_t tss_addr = reinterpret_cast<uint64_t>(&g_tss[0]);
    // GDT[5]にTSSの先頭アドレスを設定
    SetSystemSegment(g_gdt[kTSS >> 3], DescriptorType::kTSSAvailable, 0, tss_addr & 0xffffffff, sizeof(g_tss) - 1);
    // GDT[6]にTSSの先頭アドレスを設定
    g_gdt[(kTSS >> 3) + 1].data = tss_addr >> 32;

    // 割り込みが発生してCPL=3からCPL=0に切り替わる際（権限がアプリレベルからOSレベルに変化）TSSの値を読む必要が出ると、
    // CPUはTRレジスタが指すGDTエントリを参照してTSSを取得するので設定しておく
    LoadTR(kTSS);
}
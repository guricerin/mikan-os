#include "paging.hpp"

#include <array>

#include "asmfunc.h"

namespace {
    const uint64_t kPageSize4K = 4096;
    const uint64_t kPageSize2M = 512 * kPageSize4K;
    const uint64_t kPageSize1G = 512 * kPageSize2M;

    /// 以下の4階層が階層ページング構造を成す
    /// ページマップレベル4テーブル
    alignas(kPageSize4K) std::array<uint64_t, 512> g_pml4_table;
    /// ページディレクトリポインタテーブル
    alignas(kPageSize4K) std::array<uint64_t, 512> g_pdp_table;
    /// ページディレクトリ（これの要素がページテーブル）
    alignas(kPageSize4K)
        std::array<std::array<uint64_t, 512>, kPageDirectoryCount> g_page_directory;
} // namespace

void SetupIdentityPageTable() {
    g_pml4_table[0] = reinterpret_cast<uint64_t>(&g_pdp_table[0]) | 0x003;
    for (int i_pdpt = 0; i_pdpt < g_page_directory.size(); i_pdpt++) {
        g_pdp_table[i_pdpt] = reinterpret_cast<uint64_t>(&g_page_directory[i_pdpt]) | 0x003;
        for (int i_pd = 0; i_pd < 512; i_pd++) {
            g_page_directory[i_pdpt][i_pd] = i_pdpt * kPageSize1G + i_pd * kPageSize2M | 0x083;
        }
    }

    SetCR3(reinterpret_cast<uint64_t>(&g_pml4_table[0]));
}

void InitializePaging() {
    SetupIdentityPageTable();
}
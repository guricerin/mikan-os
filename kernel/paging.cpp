#include "paging.hpp"

#include <array>

#include "asmfunc.h"
#include "memory_manager.hpp"
#include "task.hpp"

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

    /**
     4階層ページングにおける、仮想アドレスの分割
     63:48 : 全部1 or 全部0
     47:39 : PML4の添字として利用
     38:30 : PDPの添字として利用
     29:21 : PDの添字として利用
     20:12 : PTの添字として利用
     11:0  : offset。PT[n]にこれを加算することで最終的な物理アドレスになる
     63:48の制約により、プログラマが使える仮想アドレスは
     [0x0000000000000000, 0x00007fffffffffff]と[0xffff800000000000, 0xffffffffffffffff]の両端部分のみ（カノニカルアドレス）
     どちらかにOS、逆側にアプリを配置する
     MikanOSでは下位にOS、上位にアプリを配置する
    */
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

void ResetCR3() {
    SetCR3(reinterpret_cast<uint64_t>(&g_pml4_table[0]));
}

namespace {
    /// 必要に応じて新たなページング構造を生成してエントリに設定
    WithError<PageMapEntry*> SetNewPageMapIfNotPresent(PageMapEntry& entry) {
        // 有効な値が設定済み
        if (entry.bits.present) {
            return {entry.Pointer(), MAKE_ERROR(Error::kSuccess)};
        }

        auto [child_map, err] = NewPageMap();
        if (err) {
            return {nullptr, err};
        }

        entry.SetPointer(child_map);
        entry.bits.present = 1;

        return {child_map, MAKE_ERROR(Error::kSuccess)};
    }

    /// 指定階層を設定
    /// page_map : 階層ページング構造の物理アドレス
    /// page_map_level : 設定対象のページング階層
    /// addr : LOADセグメントを配置する先頭アドレス
    /// num_4kPages : 4KiBページ単位のセグメントの大きさ
    /// ret : 未処理のページ数
    WithError<size_t> SetupPageMap(PageMapEntry* page_map, int page_map_level, LinearAddress4Level addr, size_t num_4kPages) {
        while (num_4kPages > 0) {
            // 仮想アドレスの指定した階層の値
            const auto entry_index = addr.Part(page_map_level);

            auto [child_map, err] = SetNewPageMapIfNotPresent(page_map[entry_index]);
            if (err) {
                return {num_4kPages, err};
            }
            page_map[entry_index].bits.writable = 1;
            // 権限レベルが最低でも命令フェッチを許可する
            // この関数はアプリ用の階層ページング構造を設定するものなので、OS領域のuserビットは0のまま変更していない
            // -> OSのメモリ空間は保護される
            page_map[entry_index].bits.user = 1;

            if (page_map_level == 1) {
                num_4kPages--;
            } else {
                auto [num_remain_pages, err] = SetupPageMap(child_map, page_map_level - 1, addr, num_4kPages);
                if (err) {
                    return {num_4kPages, err};
                }
                num_4kPages = num_remain_pages;
            }

            if (entry_index == 511) {
                break;
            }

            addr.SetPart(page_map_level, entry_index + 1);
            for (int level = page_map_level - 1; level >= 1; level--) {
                addr.SetPart(level, 0);
            }
        }

        return {num_4kPages, MAKE_ERROR(Error::kSuccess)};
    }

    /// 指定階層のページング構造内のエントリをすべて破棄
    Error CleanPageMap(PageMapEntry* page_map, int page_map_level) {
        for (int i = 0; i < 512; i++) {
            auto entry = page_map[i];
            if (!entry.bits.present) {
                continue;
            }

            // 深さ優先探索
            if (page_map_level > 1) {
                if (auto err = CleanPageMap(entry.Pointer(), page_map_level - 1)) {
                    return err;
                }
            }

            const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
            const FrameID map_frame{entry_addr / kBytesPerFrame};
            if (auto err = g_memory_manager->Free(map_frame, 1)) {
                return err;
            }
            page_map[i].data = 0;
        }

        return MAKE_ERROR(Error::kSuccess);
    }
} // namespace

/// 新たなページング構造を生成
WithError<PageMapEntry*> NewPageMap() {
    auto frame = g_memory_manager->Allocate(1);
    if (frame.error) {
        return {nullptr, frame.error};
    }

    auto e = reinterpret_cast<PageMapEntry*>(frame.value.Frame());
    memset(e, 0, sizeof(uint64_t) * 512);
    return {e, MAKE_ERROR(Error::kSuccess)};
}

Error FreePageMap(PageMapEntry* table) {
    const FrameID frame{reinterpret_cast<uintptr_t>(table) / kBytesPerFrame};
    return g_memory_manager->Free(frame, 1);
}

/// 階層ページング構造の設定
/// addr : LOADセグメントを配置する先頭アドレス
/// num_4kPages : 4KiBページ単位のセグメントの大きさ
Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages) {
    auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
    return SetupPageMap(pml4_table, 4, addr, num_4kpages).error;
}

/// アプリ用のページング構造を破棄（PML4より下層のページング構造を削除）
Error CleanPageMaps(LinearAddress4Level addr) {
    auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
    auto pdp_table = pml4_table[addr.parts.pml4].Pointer();
    pml4_table[addr.parts.pml4].data = 0;
    if (auto err = CleanPageMap(pdp_table, 3)) {
        return err;
    }

    const auto pdp_addr = reinterpret_cast<uintptr_t>(pdp_table);
    const FrameID pdp_frame{pdp_addr / kBytesPerFrame};
    return g_memory_manager->Free(pdp_frame, 1);
}

Error HandlePageFault(uint64_t error_code, uint64_t causal_addr) {
    auto& task = g_task_manager->CurrentTask();
    if (error_code & 1) { // P=1 かつページレベルの権限違反により例外発生
        return MAKE_ERROR(Error::kAlreadyAllocated);
    }

    // アプリは事前にアドレス範囲を申告しておくことで、バグによるメモリ枯渇を防ぐ
    if (causal_addr < task.DPagingBegin() || task.DPagingEnd() <= causal_addr) {
        return MAKE_ERROR(Error::kIndexOutOfRange);
    }
    // ページフォルトの原因となったページに物理フレームを割り当てる
    return SetupPageMaps(LinearAddress4Level{causal_addr}, 1);
}

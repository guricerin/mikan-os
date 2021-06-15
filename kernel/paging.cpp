#include "paging.hpp"

#include <array>

#include "asmfunc.h"
#include "logger.hpp"
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

    // スーパーバイザーによる読み込み専用ページへの書き込みを許可する
    // スーパーバイザーモード : CPL < 3 のタスク
    ResetCR3();
    SetCR0(GetCR0() & 0xfffeffff); // CR0のWPビットを0クリア
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
    /// writable : ページへの書き込み権限
    /// ret : 未処理のページ数
    WithError<size_t> SetupPageMap(PageMapEntry* page_map, int page_map_level, LinearAddress4Level addr, size_t num_4kPages, bool writable) {
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

            // いずれかの階層で writable=0 なら読み込み専用になるので、処理を単純化するため最下層のみ writable を設定
            if (page_map_level == 1) {
                page_map[entry_index].bits.writable = writable;
                num_4kPages--;
            } else {
                page_map[entry_index].bits.writable = true;
                auto [num_remain_pages, err] = SetupPageMap(child_map, page_map_level - 1, addr, num_4kPages, writable);
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
    Error CleanPageMap(PageMapEntry* page_map, int page_map_level, LinearAddress4Level addr) {
        for (int i = addr.Part(page_map_level); i < 512; i++) {
            auto entry = page_map[i];
            if (!entry.bits.present) {
                continue;
            }

            // 深さ優先探索
            if (page_map_level > 1) {
                if (auto err = CleanPageMap(entry.Pointer(), page_map_level - 1, addr)) {
                    return err;
                }
            }

            // コピーオンライトでコピーされたページは必ず writable=1 になっている
            // -> アプリの機械語（.text）や読み込み専用データ（.rodata）が含まれるLOADセグメントが読み込まれたページの物理フレームは解放しない
            if (entry.bits.writable) {
                const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
                const FrameID map_frame{entry_addr / kBytesPerFrame};
                if (auto err = g_memory_manager->Free(map_frame, 1)) {
                    return err;
                }
            }
            page_map[i].data = 0;
        }

        return MAKE_ERROR(Error::kSuccess);
    }

    /// 指定アドレスを含むファイルマッピングを探す
    const FileMapping* FindFileMapping(const std::vector<FileMapping>& fmaps, uint64_t causal_addr) {
        for (const FileMapping& m : fmaps) {
            if (m.vaddr_begin <= causal_addr && causal_addr < m.vaddr_end) {
                return &m;
            }
        }
        return nullptr;
    }

    /// 指定ページを作成しファイルをコピーする
    Error PreparePageCache(IFileDescriptor& fd, const FileMapping& m, uint64_t causal_vaddr) {
        LinearAddress4Level page_vaddr{causal_vaddr};
        page_vaddr.parts.offset = 0;
        // 4KiBページ作成
        if (auto err = SetupPageMaps(page_vaddr, 1)) {
            return err;
        }

        const long file_offset = page_vaddr.value - m.vaddr_begin;
        void* page_cache = reinterpret_cast<void*>(page_vaddr.value);
        // ページ（が指すフレーム）にファイルデータをコピー
        fd.Load(page_cache, 4096, file_offset);
        return MAKE_ERROR(Error::kSuccess);
    }

    /// 物理フレームを書き込み可でマップする
    /// content : 物理フレームを指している
    Error SetPageContent(PageMapEntry* table, int part, LinearAddress4Level addr, PageMapEntry* content) {
        if (part == 1) {
            const auto i = addr.Part(part);
            table[i].SetPointer(content);
            table[i].bits.writable = 1;
            // 階層ページング構造の一部を書き換えた場合（コピーオンライト）は古い履歴を参照し続けてしまうので、無効化する
            InvalidateTLB(addr.value);
            return MAKE_ERROR(Error::kSuccess);
        }

        const auto i = addr.Part(part);
        return SetPageContent(table[i].Pointer(), part - 1, addr, content);
    }

    /// 4KiBページをコピーして書き込み可でマップする
    Error CopyOnePage(uint64_t causal_addr) {
        auto [p, err] = NewPageMap();
        if (err) {
            return err;
        }

        const auto aligned_addr = causal_addr & 0xfffffffffffff000;
        memcpy(p, reinterpret_cast<const void*>(aligned_addr), 4096);
        return SetPageContent(reinterpret_cast<PageMapEntry*>(GetCR3()), 4, LinearAddress4Level{causal_addr}, p);
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

/// 階層ページング構造の設定。仮想アドレス（ページ）を物理アドレス（フレーム）に割り当てる。
/// addr : データを配置する先頭アドレス
/// num_4kPages : 4KiBページ単位のセグメントの大きさ
Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages, bool writable) {
    auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
    return SetupPageMap(pml4_table, 4, addr, num_4kpages, writable).error;
}

/// アプリ用のページング構造を破棄（PML4より下層のページング構造を削除）
Error CleanPageMaps(LinearAddress4Level addr) {
    auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
    return CleanPageMap(pml4_table, 4, addr);
}

/// 階層ページング構造の浅いコピーを行う
/// PML4, PDP, PD, PTについては新規のテーブルを作成して値をコピーするが、PTが指す物理フレームのコピーは行わない
Error CopyPageMaps(PageMapEntry* dest, PageMapEntry* src, int part, int start) {
    if (part == 1) {
        for (int i = start; i < 512; i++) {
            if (!src[i].bits.present) {
                continue;
            }
            dest[i] = src[i];
            dest[i].bits.writable = 0;
        }
        return MAKE_ERROR(Error::kSuccess);
    }

    for (int i = start; i < 512; i++) {
        if (!src[i].bits.present) {
            continue;
        }
        auto [table, err] = NewPageMap();
        if (err) {
            return err;
        }
        dest[i] = src[i];
        dest[i].SetPointer(table);
        if (auto err = CopyPageMaps(table, src[i].Pointer(), part - 1, 0)) {
            return err;
        }
    }

    return MAKE_ERROR(Error::kSuccess);
}

Error HandlePageFault(uint64_t error_code, uint64_t causal_addr) {
    auto& task = g_task_manager->CurrentTask();
    const bool present = (error_code >> 0) & 1;
    const bool rw = (error_code >> 1) & 1;
    const bool user = (error_code >> 2) & 1;
    if (present && rw && user) { // ページは存在するが読み込み専用なのでユーザーモードの書き込みが失敗
        // コピーオンライト
        CopyOnePage(causal_addr);
    } else if (present) { // ページは存在するがページレベルの権限違反により例外発生
        return MAKE_ERROR(Error::kAlreadyAllocated);
    }

    // デマンドページングの処理
    if (task.DPagingBegin() <= causal_addr && causal_addr < task.DPagingEnd()) {
        // ページフォルトの原因となったページに物理フレームを割り当てる
        return SetupPageMaps(LinearAddress4Level{causal_addr}, 1);
    }

    // メモリマップドファイルの処理
    if (auto m = FindFileMapping(task.FileMaps(), causal_addr)) {
        return PreparePageCache(*task.Files()[m->fd], *m, causal_addr);
    }

    // アプリは事前にアドレス範囲を申告しておくことで、バグによるメモリ枯渇を防ぐ
    return MAKE_ERROR(Error::kIndexOutOfRange);
}

/// ページ : リニアアドレス上の区画
/// ページフレーム : 物理アドレス上の区画
#pragma once

#include <array>
#include <limits>

#include "error.hpp"

namespace {
    // ユーザー定義リテラル
    constexpr unsigned long long operator""_KiB(unsigned long long kib) {
        return kib * 1024;
    }

    constexpr unsigned long long operator""_MiB(unsigned long long mib) {
        return mib * 1024_KiB;
    }

    constexpr unsigned long long operator""_GiB(unsigned long long gib) {
        return gib * 1024_MiB;
    }
} // namespace

/// 物理メモリフレーム（ページフレーム）1つの大きさ（byte）
static const auto kBytesPerFrame{4_KiB};

/// ページフレーム番号
class FrameID {
public:
    explicit FrameID(size_t id) : id_{id} {}
    size_t ID() const { return id_; }
    void* Frame() const { return reinterpret_cast<void*>(id_ * kBytesPerFrame); }

private:
    size_t id_;
};

/// 未定義のページフレーム番号（null番人）
static const FrameID kNullFrame{std::numeric_limits<size_t>::max()};

/// ビットマップ配列を用いてページフレーム単位でメモリ管理するクラス
/// 配列alloc_map_の各ビットがページフレームに対応し、0なら空き、1なら使用中
/// alloc_map_[n]のmビット目が対応する物理アドレスは次の式で求まる
///     kFrameBytes * (n * kBitsPerMapLine + m)
class BitmapMemoryManager {
public:
    /// このメモリ管理クラスで扱える最大の物理メモリ量（byte）
    static const auto kMaxPhysicalMemoryBytes{128_GiB};
    /// kMaxPhysicalMemoryBytesまでの物理メモリを扱うために必要なページフレーム数
    static const auto kFrameCount{kMaxPhysicalMemoryBytes / kBytesPerFrame};

    /// ビットマップ配列の要素型
    using MapLineType = unsigned long;
    static const auto kBitsPerMapLine{8 * sizeof(MapLineType)};

    BitmapMemoryManager();

    /// 要求されたフレーム数の領域を確保して先頭のフレームIDを返す
    WithError<FrameID> Allocate(size_t num_frames);
    Error Free(FrameID start_frame, size_t num_frames);
    // 使用中領域を設定（使用しているのがUEFIなのかこのメモリマネージャーなのかは問わない）
    void MarkAllocated(FrameID start_frame, size_t num_frames);

    /// このメモリマネージャーで扱うメモリ範囲を設定
    /// この呼出以降、Allocateによるメモリ割り当ては設定された範囲内でのみ行われる
    void SetMemoryRange(FrameID range_begin, FrameID range_end);

private:
    /// 1ページフレームを1ビットで表したビットマップ
    std::array<MapLineType, kFrameCount / kBitsPerMapLine> alloc_map_;

    /// このメモリマネージャーで扱うメモリ範囲 : [range_start_, range_end_)
    FrameID range_begin_;
    FrameID range_end_;

    bool GetBit(FrameID framne) const;
    void SetBit(FrameID frame, bool allocated);
};

Error InitializeHeap(BitmapMemoryManager& memory_manager);
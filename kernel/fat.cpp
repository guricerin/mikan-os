#include "fat.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>

namespace {
    /// 指定パスを '/' で区切った最初の要素をpath_elemにコピー
    /// 指定パスの次の要素を返す
    /// 最初の要素の末尾に '/' があるかを返す
    std::pair<const char*, bool> NextPathElement(const char* path, char* path_elem) {
        const char* next_slash = strchr(path, '/');
        if (next_slash == nullptr) {
            strcpy(path_elem, path);
            return {nullptr, false};
        }

        const auto elem_len = next_slash - path;
        strncpy(path_elem, path, elem_len);
        path_elem[elem_len] = '\0';
        return {&next_slash[1], true};
    }
} // namespace

namespace fat {
    BPB* g_boot_volume_image;
    unsigned long g_bytes_per_cluster;

    void Initialize(void* volume_iamge) {
        g_boot_volume_image = reinterpret_cast<BPB*>(volume_iamge);
        g_bytes_per_cluster = static_cast<unsigned long>(g_boot_volume_image->bytes_per_sector) * g_boot_volume_image->sectors_per_cluster;
    }

    uintptr_t GetClusterAddr(unsigned long cluster) {
        unsigned long sector_num =
            g_boot_volume_image->reserved_sector_count + g_boot_volume_image->num_fats * g_boot_volume_image->fat_size_32 + (cluster - 2) * g_boot_volume_image->sectors_per_cluster;
        uintptr_t offset = sector_num * g_boot_volume_image->bytes_per_sector;
        return reinterpret_cast<uintptr_t>(g_boot_volume_image) + offset;
    }

    void ReadName(const DirectoryEntry& entry, char* base, char* ext) {
        memcpy(base, &entry.name[0], 8);
        base[8] = 0; // null文字
        // 末尾の0x20（空白文字）を除去
        for (int i = 7; i >= 0 && base[i] == 0x20; i--) {
            base[i] = 0;
        }

        memcpy(ext, &entry.name[8], 3);
        ext[3] = 0; // null文字
        for (int i = 2; i >= 0 && ext[i] == 0x20; i--) {
            ext[i] = 0;
        }
    }

    void FormatName(const DirectoryEntry& entry, char* dest) {
        char ext[5] = ".";
        ReadName(entry, dest, &ext[1]);
        if (ext[1]) {
            strcat(dest, ext);
        }
    }

    unsigned long NextCluster(unsigned long cluster) {
        uintptr_t fat_offset = g_boot_volume_image->reserved_sector_count * g_boot_volume_image->bytes_per_sector;
        uint32_t* fat = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(g_boot_volume_image) + fat_offset);
        uint32_t next = fat[cluster];
        if (next >= 0x0ffffff8ul) {
            return kEndOfClusterchain;
        }
        return next;
    }

    std::pair<DirectoryEntry*, bool> FindFile(const char* path, unsigned long directory_cluster) {
        if (path[0] == '/') { // 絶対パス
            directory_cluster = g_boot_volume_image->root_cluster;
            path++;
        } else if (directory_cluster == 0) {
            directory_cluster = g_boot_volume_image->root_cluster;
        }

        // ex.
        // 1回目: path = "efi/boot" -> path_elem = "efi", path_last = false
        // 2回目: path = "boot" -> path_elem = "boot", path_last = true
        char path_elem[13];
        const auto [next_path, post_slash] = NextPathElement(path, path_elem);
        // path_elemにコピーされた文字列がパスの末尾かどうか
        const bool path_last = next_path == nullptr || next_path[0] == '\0';

        while (directory_cluster != kEndOfClusterchain) {
            auto dir = GetSectorByCluster<DirectoryEntry>(directory_cluster);
            // path_elemと一致する名前のエントリを探索
            for (int i = 0; i < g_bytes_per_cluster / sizeof(DirectoryEntry); i++) {
                if (dir[i].name[0] == 0x00) {
                    goto not_found;
                } else if (!NameIsEqual(dir[i], path_elem)) {
                    continue;
                }

                // path_elemと一致するエントリをみつけた

                if (dir[i].attr == Attribute::kDirectory && !path_last) { // 1段潜る
                    return FindFile(next_path, dir[i].FirstCluster());
                } else {
                    // dir[i]がディレクトリではないか、パスの末尾に来たので探索をやめる
                    return {&dir[i], post_slash};
                }
            }

            // ディレクトリが複数クラスタにまたがっていても対応できるようにしている
            directory_cluster = NextCluster(directory_cluster);
        }

    not_found:
        return {nullptr, post_slash};
    }

    bool NameIsEqual(const DirectoryEntry& entry, const char* name) {
        unsigned char name83[11];
        memset(name83, 0x20, sizeof(name83));

        int i = 0, i83 = 0;
        for (; name[i] != 0 && i83 < sizeof(name83); i++, i83++) {
            if (name[i] == '.') {
                i83 = 7;
                continue;
            }
            // 大文字変換
            name83[i83] = toupper(name[i]);
        }

        return memcmp(entry.name, name83, sizeof(name83)) == 0;
    }

    size_t LoadFile(void* buf, size_t len, const DirectoryEntry& entry) {
        auto is_valid_cluster = [](uint32_t c) {
            return c != 0 && c != fat::kEndOfClusterchain;
        };

        auto cluster = entry.FirstCluster();
        const auto buf_uint8 = reinterpret_cast<uint8_t*>(buf);
        const auto buf_end = buf_uint8 + len;
        auto p = buf_uint8;

        // クラスタチェーンを辿る
        while (is_valid_cluster(cluster)) {
            if (g_bytes_per_cluster >= buf_end - p) {
                memcpy(p, GetSectorByCluster<uint8_t>(cluster), buf_end - p);
                return len;
            }
            memcpy(p, GetSectorByCluster<uint8_t>(cluster), g_bytes_per_cluster);
            p += g_bytes_per_cluster;
            // 複数クラスタにまたがる場合にも対応
            cluster = NextCluster(cluster);
        }

        return p - buf_uint8;
    }

    FileDescriptor::FileDescriptor(DirectoryEntry& fat_entry) : fat_entry_{fat_entry} {
    }

    size_t FileDescriptor::Read(void* buf, size_t len) {
        if (rd_cluster_ == 0) {
            rd_cluster_ = fat_entry_.FirstCluster();
        }

        uint8_t* buf8 = reinterpret_cast<uint8_t*>(buf);
        len = std::min(len, fat_entry_.file_size - rd_off_);

        size_t total = 0;
        while (total < len) {
            uint8_t* sec = GetSectorByCluster<uint8_t>(rd_cluster_);
            size_t n = std::min(len - total, g_bytes_per_cluster - rd_cluster_off_);
            memcpy(&buf8[total], &sec[rd_cluster_off_], n);
            total += n;

            rd_cluster_off_ += n;
            // クラスタをまたがるファイルに対応
            if (rd_cluster_off_ == g_bytes_per_cluster) {
                rd_cluster_ = NextCluster(rd_cluster_);
                rd_cluster_off_ = 0;
            }
        }

        // rd_off_はメソッド呼び出し前よりlenバイトだけ進む
        rd_off_ += total;
        return total;
    }
} // namespace fat

#include "fat.hpp"

#include <cstring>

namespace fat {
    BPB* g_boot_volume_image;

    void Initialize(void* volume_iamge) {
        g_boot_volume_image = reinterpret_cast<BPB*>(volume_iamge);
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
} // namespace fat

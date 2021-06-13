/// FATファイルシステム
/// FAT : ファイルアロケーションテーブル

#pragma once

#include <cstddef>
#include <cstdint>

#include "error.hpp"
#include "file.hpp"

namespace fat {
    /// パーティション : ブロックデバイスを複数に分割した1つの領域
    /// PBR (Partition Boot Record) : パーティションの先頭1ブロック
    /// BPB (BIOS Parameter Block) : PBRに含まれる特に重要な領域
    /// セクタ : FATにおけるブロックのこと
    /// クラスタ : いくつかのブロックをまとめたもの
    /// P.400
    struct BPB {
        uint8_t jump_boot[3];           // プログラムへのジャンプ命令
        char oem_name[8];               // 任意の8byte文字列
        uint16_t bytes_per_sector;      // バイト数 / ブロック
        uint8_t sectors_per_cluster;    // ブロック数 / クラスタ
        uint16_t reserved_sector_count; // ボリューム先頭からの予約ブロック数
        uint8_t num_fats;               // FATの数
        uint16_t root_entry_count;      // ルートディレクトリエントリ数（FAT32では値は常に0）
        uint16_t total_sectors_16;      // ボリューム全体の総ブロック数（FAT32では値は常に0）
        uint8_t media;                  // （現在は不使用）
        uint16_t fat_size_16;           // ブロック数 / FAT（FAT32では値は常に0）
        uint16_t sectors_per_track;     // ブロック数 / トラック
        uint16_t num_heads;             // ヘッド数
        uint32_t hidden_sectors;        // 隠しブロック数
        uint32_t total_sectors_32;      // ボリューム全体の総ブロック数
        uint32_t fat_size_32;           // ブロック数 / FAT
        uint16_t ext_flags;             // FATの冗長化に関するフラグ
        uint16_t fs_version;            // ファイルシステムバージョン
        uint32_t root_cluster;          // ルートディレクトリの開始クラスタ
        uint16_t fs_info;               // FSINFO構造体の開始ブロック番号
        uint16_t backup_boot_sector;    // ブートセクタのコピーが置かれているブロック番号
        uint8_t reserved[12];           // 予約領域
        uint8_t drive_number;           // BIOSのINT 0x13で使うドライブ番号
        uint8_t reserved1;              // 予約領域
        uint8_t boot_signature;         // 拡張シグネチャ
        uint32_t volume_id;             // ボリュームのシリアルナンバー
        char volume_lavel[11];          // ボリュームラベル
        char fs_type[8];                // ファイルシステム種別
    } __attribute__((packed));

    /// FATファイルシステムのファイル属性
    enum class Attribute : uint8_t {
        kReadOnly = 0x01,  // 読み込み専用
        kHidden = 0x02,    // 隠しファイル
        kSystem = 0x04,    // システムファイル
        kVolumeID = 0x08,  // ボリューム名
        kDirectory = 0x10, // ディレクトリ
        kArchive = 0x20,   // バックアップすべきフラグ
        kLongName = 0x0f,  // 長名エントリ
    };

    /// FATファイルシステムのディレクトリエントリ
    /// ファイルの内容は任意のバイト列だが、
    /// ディレクトリの内容はディレクトリエントリという32byteのデータ構造が配列状に並んでいる
    struct DirectoryEntry {
        unsigned char name[11];      // ファイルの別名
        Attribute attr;              // ファイル属性
        uint8_t ntres;               // Windows NT用の予約領域
        uint8_t create_time_tenth;   // ファイル作成時刻のmsec
        uint16_t create_time;        // ファイル作成時刻
        uint16_t create_date;        // ファイル作成日
        uint16_t last_access_date;   // 最終アクセス日
        uint16_t first_cluster_high; // 開始クラスタの上位2byte
        uint16_t write_time;         // 最終書き込み時刻
        uint16_t write_date;         // 最終書き込み日
        uint16_t first_cluster_low;  // 開始クラスタの下位2byte
        uint32_t file_size;          // ファイルのバイト数

        uint32_t FirstCluster() const {
            return first_cluster_low | (static_cast<uint32_t>(first_cluster_high) << 16);
        }
    } __attribute__((packed));

    /// ボリュームイメージ : ブロックデバイスの中身を記録したデータ
    extern BPB* g_boot_volume_image;
    /// バイト数 / クラスタ
    extern unsigned long g_bytes_per_cluster;

    void Initialize(void* volume_iamge);

    /// 指定クラスタの先頭セクタが置いてあるメモリアドレスを返す
    /// cluster : クラスタ番号（2始まり）
    uintptr_t GetClusterAddr(unsigned long cluster);

    /// 指定クラスタの先頭セクタが置いてあるメモリ領域を返す
    /// cluster : クラスタ番号（2始まり）
    template <class T>
    T* GetSectorByCluster(unsigned long cluster) {
        return reinterpret_cast<T*>(GetClusterAddr(cluster));
    }

    /// ディレクトリエントリの短名を、基本名と拡張子名に分割して取得
    /// パディングされた空白文字（0x20）は除去され、null終端される
    /// entry : ファイル名を得る対象のディレクトリエントリ
    /// base : 拡張子を除いたファイル名（9byte以上の配列）
    /// ext : 拡張子（4byte以上の配列）
    void ReadName(const DirectoryEntry& entry, char* base, char* ext);

    /// ディレクトリエントリの短名をdestにコピー
    /// 短名の拡張子が空なら "<base>"を、空でなければ"<base>.<ext>"をコピー
    void FormatName(const DirectoryEntry& entry, char* dest);

    static const unsigned long kEndOfClusterchain = 0x0ffffffflu;

    /// 指定クラスタの次のクラスタ番号を返す
    unsigned long NextCluster(unsigned long cluster);

    /// 指定ディレクトリからファイルを探す
    /// path : 8+3形式のファイル名（大文字小文字は区別しない）
    /// directory_cluster : ディレクトリの開始クラスタ（省略するとルートから検索）
    /// return : ファイルorディレクトリを表すエントリ、末尾スラッシュを示すフラグ
    ///     エントリが見つからなければnullptr
    ///     エントリの直後にスラッシュがあればtrue
    ///     パスの途中のエントリがファイルであれば探索を諦め、そのエントリとtrueを返す
    std::pair<DirectoryEntry*, bool> FindFile(const char* path, unsigned long directory_cluster = 0);

    /// 指定のファイル名と一致 : true
    bool NameIsEqual(const DirectoryEntry& entry, const char* name);

    /// 指定ファイルの内容をバッファへコピー
    /// buf : コピー先
    /// len : バッファの大きさ（byte単位）
    /// entry : ファイルを表すディレクトリエントリ
    /// ret : 読み込んだバイト数
    size_t LoadFile(void* buf, size_t len, const DirectoryEntry& entry);

    bool IsEndOfClusterchain(unsigned long cluster);

    /// ボリュームイメージの中のFAT構造の先頭アドレスを取得
    uint32_t* GetFAT();

    /// 指定クラスタ数だけクラスタチェーンを伸長
    /// eoc_cluster : 伸長したいクラスタチェーンに属するいすれかのクラスタ番号
    /// n : 伸長後のチェーンにおける最後尾のクラスタ番号
    unsigned long ExtendCluster(unsigned long eoc_cluster, size_t n);

    /// 指定ディレクトリの空きエントリを1つ返す
    DirectoryEntry* AllocateEntry(unsigned long dir_cluster);

    /// ディレクトリエントリに短ファイル名を設定
    /// entry : 対象のディレクトリエントリ
    /// name : 基本名と拡張子を . で結合したファイル名
    void SetFileName(DirectoryEntry& entry, const char* name);

    /// 指定パスにファイルエントリを作成
    /// return : 新規作成されたファイルエントリ
    WithError<DirectoryEntry*> CreateFile(const char* path);

    /// 各タスクがアクセスするファイルをOSカーネルが識別するための識別子、整数
    /// この型ではFAT上のファイルを扱う
    class FileDescriptor : public IFileDescriptor {
    public:
        explicit FileDescriptor(DirectoryEntry& fat_entry);
        size_t Read(void* buf, size_t len) override;

    private:
        /// ファイルへの参照
        DirectoryEntry& fat_entry_;
        /// アイル先頭からの読み込みオフセット（byte単位）
        size_t rd_off_ = 0;
        /// rd_off_が指す位置に対応するクラスタ番号
        unsigned long rd_cluster_ = 0;
        /// クラスタ先頭からのオフセット（byte単位）
        size_t rd_cluster_off_ = 0;
    };
} // namespace fat

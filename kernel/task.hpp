/// タスク管理、コンテキスト切り替え

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "error.hpp"
#include "fat.hpp"
#include "message.hpp"

/// コンテキスト : タスクの実行バイナリ、コマンドライン引数、環境変数、スタックメモリ、各レジスタの値など
/// コンテキストの切替時に値の保存と復帰に必要なレジスタをすべて含む
struct TaskContext {
    uint64_t cr3, rip, rflags, reserved;             // offset 0x00
    uint64_t cs, ss, fs, gs;                         // offset 0x20
    uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp; // offset 0x40
    uint64_t r8, r9, r19, r11, r12, r13, r14, r15;   // offset 0x80
    std::array<uint8_t, 512> fxsave_area;            // offset 0xc0
} __attribute__((packed));

using TaskFunc = void(uint64_t, int64_t);

class TaskManager;

/// ファイルの内容を仮想アドレス空間の連続した領域にマッピング
struct FileMapping {
    /// ファイルディスクリプタ
    int fd;
    /// 仮想アドレス範囲
    uint64_t vaddr_begin, vaddr_end;
};

/// タスク : 動作中のプログラム。処理単位。
class Task {
public:
    /// 優先度。数字が大きいほど高い。
    static const int kDefaultLevel = 1;
    /// 32KiB
    static const size_t kDefaultStackBytes = 8 * 4096;

    Task(uint64_t id);
    /// f : 実際に実行されるタスク（関数）
    Task& InitContext(TaskFunc* f, int64_t data);
    TaskContext& Context();
    uint64_t& OSStackPointer();
    uint64_t ID() const;
    Task& Sleep();
    Task& Wakeup();
    /// イベントメッセージが通知されたら起こす
    void SendMessage(const Message& msg);
    /// メッセージを取得
    std::optional<Message> ReceiveMessage();
    std::vector<std::shared_ptr<IFileDescriptor>>& Files();
    uint64_t DPagingBegin() const;
    void SetDPagingBegin(uint64_t v);
    uint64_t DPagingEnd() const;
    void SetDPagingEnd(uint64_t v);
    uint64_t FileMapEnd() const;
    void SetFileMapEnd(uint64_t v);
    std::vector<FileMapping>& FileMaps();

    int Level() const { return level_; }
    bool Running() const { return running_; }

private:
    uint64_t id_;
    /// スタック領域
    std::vector<uint64_t> stack_;
    alignas(16) TaskContext context_;
    /// OS用スタックポインタ（アプリ終了時からの復帰に必要）
    uint64_t os_stack_pointer_;
    /// 割り込みメッセージキュー
    std::deque<Message> msgs_;
    unsigned int level_{kDefaultLevel};
    /// 実行可能状態（待機列に並んでいる） : true
    bool running_{false};
    /// ファイルディスクリプタをタスク毎に持たせる
    /// -> 番号が他のタスクとだぶっても大丈夫
    std::vector<std::shared_ptr<IFileDescriptor>> files_{};
    /// デマンドページングの仮想アドレス範囲
    uint64_t dpaging_begin_{0}, dpaging_end_{0};
    /// メモリマップドファイルに利用される仮想アドレス範囲
    uint64_t file_map_end_{0};
    std::vector<FileMapping> file_maps_{};

    Task& SetLevel(int level) {
        level_ = level;
        return *this;
    }
    Task& SetRunning(bool running) {
        running_ = running;
        return *this;
    }

    friend TaskManager;
};

/// 複数のタスクを管理
class TaskManager {
public:
    static const int kMaxLevel = 3;
    TaskManager();
    /// 待機列には追加しない
    Task& NewTask();
    /// タスク切替え
    void SwitchTask(const TaskContext& current_ctx);
    /// タスクをスリープ状態にする（待機列から除外）
    void Sleep(Task* task);
    Error Sleep(uint64_t id);
    /// タスクを実行可能状態にする（待機列に復帰）
    void Wakeup(Task* task, int level = -1);
    Error Wakeup(uint64_t id, int level = -1);
    /// 指定のタスクに割り込みメッセージを通知し、待機列に復帰させる
    Error SendMessage(uint64_t id, const Message& msg);
    /// 現在実行中のタスク
    Task& CurrentTask();

private:
    /// タスク一覧
    std::vector<std::unique_ptr<Task>> tasks_{};
    /// 最後に生成されたタスクのID
    /// 1 : メインタスク（KernelMainStack()）
    uint64_t latest_id_{0};
    /// 優先度別のタスクの待機列（ランキュー）
    /// 先頭を現在実行中のタスクとする
    /// あるタスクより優先度の低いタスクは、そのタスクがスリープするか同じ優先度まで下がらない限り実行されない
    std::array<std::deque<Task*>, kMaxLevel + 1> running_{};
    /// 現在実行中のタスクが属する優先度
    int current_level_{kMaxLevel};
    /// 次回のタスク切替え時に現在の実行レベルを変更 : true
    bool level_changed_{false};

    void ChangeLevelRunning(Task* task, int level);
    /// ランキューの戦闘要素を末尾に移動
    Task* RotateCurrentRunQueue(bool current_sleep);
};

extern TaskManager* g_task_manager;
constexpr uint64_t kMainTaskID = 1;

void InitializeTask();

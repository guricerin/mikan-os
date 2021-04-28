/// タスク管理、コンテキスト切り替え

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

/// コンテキスト : タスクの実行バイナリ、コマンドライン引数、環境変数、スタックメモリ、各レジスタの値など
/// コンテクストの切替時に値の保存と復帰に必要なレジスタをすべて含む
struct TaskContext {
    uint64_t cr3, rip, rflags, reserved;             // offset 0x00
    uint64_t cs, ss, fs, gs;                         // offset 0x20
    uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp; // offset 0x40
    uint64_t r8, r9, r19, r11, r12, r13, r14, r15;   // offset 0x80
    std::array<uint8_t, 512> fxsave_area;            // offset 0xc0
} __attribute__((packed));

using TaskFunc = void(uint64_t, int64_t);

/// タスク : 動作中のプログラム。処理単位。
class Task {
public:
    static const size_t kDefaultStackBytes = 4096;

    Task(uint64_t id);
    /// f : 実際に実行されるタスク（関数）
    Task& InitContext(TaskFunc* f, int64_t data);
    TaskContext& Context();

private:
    uint64_t id_;
    /// スタック領域
    std::vector<uint64_t> stack_;
    alignas(16) TaskContext context_;
};

/// 複数のタスクを管理
class TaskManager {
public:
    TaskManager();
    Task& NewTask();
    void SwitchTask();

private:
    /// タスク一覧
    std::vector<std::unique_ptr<Task>> tasks_{};
    /// 最後に生成されたタスクのID
    uint64_t latest_id_{0};
    /// 現在実行中のタスク（tasks_のインデックス用）
    size_t current_task_index_{0};
};

extern TaskManager* g_task_manager;

void InitializeTask();
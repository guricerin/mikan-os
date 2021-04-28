/// タスク管理、コンテキスト切り替え

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "error.hpp"
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

/// タスク : 動作中のプログラム。処理単位。
class Task {
public:
    static const size_t kDefaultStackBytes = 4096;

    Task(uint64_t id);
    /// f : 実際に実行されるタスク（関数）
    Task& InitContext(TaskFunc* f, int64_t data);
    TaskContext& Context();
    uint64_t ID() const;
    Task& Sleep();
    Task& Wakeup();
    /// イベントメッセージが通知されたら起こす
    void SendMessage(const Message& msg);
    /// メッセージを取得
    std::optional<Message> ReceiveMessage();

private:
    uint64_t id_;
    /// スタック領域
    std::vector<uint64_t> stack_;
    alignas(16) TaskContext context_;
    /// 割り込みメッセージキュー
    std::deque<Message> msgs_;
};

/// 複数のタスクを管理
class TaskManager {
public:
    TaskManager();
    /// 待機列には追加しない
    Task& NewTask();
    /// タスク切替え
    void SwitchTask(bool current_sleep = false);
    /// タスクをスリープ状態にする（待機列から除外）
    void Sleep(Task* task);
    Error Sleep(uint64_t id);
    /// タスクを実行可能状態にする（待機列に復帰）
    void Wakeup(Task* task);
    Error Wakeup(uint64_t id);
    /// 指定のタスクに割り込みメッセージを通知し、待機列に復帰させる
    Error SendMessage(uint64_t id, const Message& msg);
    /// 現在実行中のタスク
    Task& CurrentTask();

private:
    /// タスク一覧
    std::vector<std::unique_ptr<Task>> tasks_{};
    /// 最後に生成されたタスクのID
    uint64_t latest_id_{0};
    /// タスクの待機列（ランキュー）
    /// 先頭を現在実行中のタスクとする
    std::deque<Task*> running_{};
};

extern TaskManager* g_task_manager;

void InitializeTask();
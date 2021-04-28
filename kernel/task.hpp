/// タスク管理、コンテキスト切り替え

#pragma once

#include <array>
#include <cstdint>

/// コンテクストの切替時に値の保存と復帰に必要なレジスタをすべて含む
struct TaskContext {
    uint64_t cr3, rip, rflags, reserved;             // offset 0x00
    uint64_t cs, ss, fs, gs;                         // offset 0x20
    uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp; // offset 0x40
    uint64_t r8, r9, r19, r11, r12, r13, r14, r15;   // offset 0x80
    std::array<uint8_t, 512> fxsave_area;            // offset 0xc0
} __attribute__((packed));

extern TaskContext g_task_b_ctx, g_task_a_ctx;

void SwitchTask();
void InitializeTask();
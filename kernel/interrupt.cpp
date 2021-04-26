#include "interrupt.hpp"

#include "asmfunc.h"
#include "segment.hpp"

std::array<InterruptDescriptor, 256> g_idt;

void SetIDTEntry(InterruptDescriptor& desc,
                 InterruptDescriptorAttribute attr,
                 uint64_t offset,
                 uint16_t segment_selector) {
    desc.attr = attr;
    desc.offset_low = offset & 0xffffu;
    desc.offset_middle = (offset >> 16) & 0xffffu;
    desc.offset_high = offset >> 32;
    desc.segment_selector = segment_selector;
}

void NotifyEndOfInterrupt() {
    // [0xfee00000, 0xfee00400] はCPUのレジスタが配置されている
    volatile auto end_of_interrupt = reinterpret_cast<uint32_t*>(0xfee000b0);
    *end_of_interrupt = 0;
}

namespace {
    std::deque<Message>* g_msg_queue;

    /// xHCI用割り込みハンドラ
    __attribute__((interrupt)) void IntHandlerXHCI(InterruptFrame* frame) {
        g_msg_queue->push_back(Message{Message::kInterruptXHCI});
        NotifyEndOfInterrupt();
    }
} // namespace

void InitializeInterrupt(std::deque<Message>* msg_queue) {
    ::g_msg_queue = msg_queue;

    // IDTをCPUに登録
    SetIDTEntry(g_idt[InterruptVector::kXHCI],
                MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(IntHandlerXHCI),
                kKernelCS);
    LoadIDT(sizeof(g_idt) - 1, reinterpret_cast<uintptr_t>(&g_idt[0]));
}
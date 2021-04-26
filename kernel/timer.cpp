#include "timer.hpp"

#include "interrupt.hpp"

namespace {
    const uint32_t kCountMax = 0xffffffffu;
    /// Local APICタイマのレジスタ
    /// Local Vector Table Timer : 割り込みの設定
    volatile uint32_t& g_lvt_timer = *reinterpret_cast<uint32_t*>(0xfee00320);
    /// カウンタの初期値。0になると動作が止まる
    volatile uint32_t& g_initial_count = *reinterpret_cast<uint32_t*>(0xfee00380);
    /// カウンタの現在値
    volatile uint32_t& g_current_count = *reinterpret_cast<uint32_t*>(0xfee00390);
    /// 分周比の設定（クロックをn分の1にする）。分周比を大きくするほどカウンタの減り方がゆっくりになる
    volatile uint32_t& g_divide_config = *reinterpret_cast<uint32_t*>(0xfee003e0);
} // namespace

void InitializeLAPICTimer() {
    g_divide_config = 0b1011; // divide 1:1
    // 割り込み許可
    // Current Counter レジスタの値が0になるたびに割り込み発生
    g_lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer;
    g_initial_count = kCountMax;
}

void StartLAPICTimer() {
    g_initial_count = kCountMax;
}

uint32_t LAPICTimerElapsed() {
    return kCountMax - g_current_count;
}

void StopLAPICTimer() {
    g_initial_count = 0;
}
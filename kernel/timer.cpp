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

void InitializeLAPICTimer(std::deque<Message>& msg_queue) {
    g_timer_manager = new TimerManager(msg_queue);

    g_divide_config = 0b1011; // divide 1:1
    // 割り込み許可
    // Current Counter レジスタの値が0になるたびに割り込み発生
    g_lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer;
    g_initial_count = 0x1000000u;
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

Timer::Timer(unsigned long timeout, int value) : timeout_{timeout}, value_{value} {
}

void TimerManager::Tick() {
    tick_++;
    // タイムアウト処理
    while (true) {
        const auto& t = timers_.top();
        // 先頭がまだタイムアウトしていない場合にループから抜ける
        if (t.Timeout() > tick_) {
            break;
        }

        Message msg{Message::kTimerTimeout};
        msg.arg.timer.timeout = t.Timeout();
        msg.arg.timer.value = t.Value();
        // タイマがタイムアウトしたことをメイン関数に通知
        msg_queue_.push_back(msg);

        timers_.pop();
    }
}

TimerManager::TimerManager(std::deque<Message>& msg_queue) : msg_queue_{msg_queue} {
    // 番兵
    timers_.push(Timer{std::numeric_limits<unsigned long>::max(), -1});
}

void TimerManager::AddTimer(const Timer& timer) {
    timers_.push(timer);
}

TimerManager* g_timer_manager;

void LAPICTimerOnInterrupt() {
    g_timer_manager->Tick();
}

#include "timer.hpp"

#include "acpi.hpp"
#include "interrupt.hpp"
#include "task.hpp"

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
    g_timer_manager = new TimerManager;

    g_divide_config = 0b1011; // divide 1:1
    // 割り込み不許可
    g_lvt_timer = (0b010 << 16);

    StartLAPICTimer();
    // 100msec(0.1sec)待機
    acpi::WaitMillisecondes(100);
    const auto elapsed = LAPICTimerElapsed();
    StopLAPICTimer();

    // 1000msec(1sec)当たりのカウント数
    g_lapic_timer_freq = static_cast<unsigned long>(elapsed) * 10;

    g_divide_config = 0b1011; // divide 1:1
    // 割り込み許可
    // Current Counter レジスタの値が0になるたびに割り込み発生
    g_lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer;
    // 約10msecごとに割り込みが発生するはず
    g_initial_count = g_lapic_timer_freq / kTimerFreq;
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

TimerManager::TimerManager() {
    // 番兵
    timers_.push(Timer{std::numeric_limits<unsigned long>::max(), -1});
}

bool TimerManager::Tick() {
    tick_++;

    bool task_timer_timeout = false;
    // タイムアウト処理
    while (true) {
        const auto& t = timers_.top();
        // 先頭がまだタイムアウトしていない場合にループから抜ける
        if (t.Timeout() > tick_) {
            break;
        }

        if (t.Value() == kTaskTimerValue) {
            task_timer_timeout = true;
            timers_.pop();
            timers_.push(Timer{tick_ + kTaskTimerPeriod, kTaskTimerValue});
            continue;
        }

        Message msg{Message::kTimerTimeout};
        msg.arg.timer.timeout = t.Timeout();
        msg.arg.timer.value = t.Value();
        // タイマがタイムアウトしたことをメイン関数に通知
        g_task_manager->SendMessage(1, msg);

        timers_.pop();
    }

    return task_timer_timeout;
}

void TimerManager::AddTimer(const Timer& timer) {
    timers_.push(timer);
}

TimerManager* g_timer_manager;
unsigned long g_lapic_timer_freq;

void LAPICTimerOnInterrupt() {
    const bool task_timer_timeout = g_timer_manager->Tick();
    // タスク切り替えの前にコールしておかないと、タスク切り替え後にタイマ割り込みがこなくなる
    NotifyEndOfInterrupt();

    if (task_timer_timeout) {
        g_task_manager->SwitchTask();
    }
}

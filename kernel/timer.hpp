/// Local APICタイマを操作する
/// タイマ : 一定時間ごとにカウンタの数値を増減するレジスタをもつ
/// Local APIC : CPU内部に実装されている割り込みコントローラー
/// Local APICタイマ : Local APICのタイマ。CPUコア1つにつき1つのみ搭載。
#pragma once

#include "message.hpp"
#include <cstdint>
#include <queue>
#include <vector>

void InitializeLAPICTimer(std::deque<Message>& msg_queue);
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();

/// Local APICタイマの1カウントを基準とした、論理的なタイマ
class Timer {
public:
    Timer(unsigned long timeout, int value);
    unsigned long Timeout() const { return timeout_; }
    int Value() const { return value_; }

private:
    /// タイムアウト時刻
    /// TimeManager::tick_の値以下ならタイムアウトしたと見做す
    unsigned long timeout_;
    /// タイムアウト時に送信する値
    int value_;
};

/// タイマー優先度を比較。タイムアウトが遠いほど優先度低
inline bool operator<(const Timer& lhs, const Timer& rhs) {
    return lhs.Timeout() > rhs.Timeout();
}

/// タイマの割り込み回数を管理
class TimerManager {
public:
    TimerManager(std::deque<Message>& msg_queue);
    void AddTimer(const Timer& timer);
    void Tick();
    unsigned long CurrentTick() const { return tick_; }

private:
    // タイマ割り込み回数
    volatile unsigned long tick_{0};
    std::priority_queue<Timer> timers_{};
    std::deque<Message>& msg_queue_;
};

extern TimerManager* g_timer_manager;

void LAPICTimerOnInterrupt();
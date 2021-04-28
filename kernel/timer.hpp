/// Local APICタイマを操作する
/// タイマ : 一定時間ごとにカウンタの数値を増減するレジスタをもつ
/// Local APIC : CPU内部に実装されている割り込みコントローラー
/// Local APICタイマ : Local APICのタイマ。CPUコア1つにつき1つのみ搭載。
#pragma once

#include "message.hpp"
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

void InitializeLAPICTimer();
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
    TimerManager();
    void AddTimer(const Timer& timer);
    /// タスク切り替え用タイマがタイムアウト : true
    bool Tick();
    unsigned long CurrentTick() const { return tick_; }

private:
    // タイマ割り込み回数
    volatile unsigned long tick_{0};
    std::priority_queue<Timer> timers_{};
};

extern TimerManager* g_timer_manager;
/// Local APICタイマの周波数（1秒あたりのカウント数）
extern unsigned long g_lapic_timer_freq;
/// 1秒間にTimerManager::Tick()をコールする回数
const int kTimerFreq = 100;

/// タスク切り替え用タイマの周期
/// 0.02secでタイムアウト
const int kTaskTimerPeriod = static_cast<int>(kTimerFreq * 0.02);
/// タスク切り替え用タイマの値（他のタイマとの識別用）
const int kTaskTimerValue = std::numeric_limits<int>::min();

void LAPICTimerOnInterrupt();
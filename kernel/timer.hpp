/// Local APICタイマを操作する
/// タイマ : 一定時間ごとにカウンタの数値を増減するレジスタをもつ
/// Local APIC : CPU内部に実装されている割り込みコントローラー
/// Local APICタイマ : Local APICのタイマ。CPUコア1つにつき1つのみ搭載。
#pragma once

#include <cstdint>

void InitializeLAPICTimer();
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();
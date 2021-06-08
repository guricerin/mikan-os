#pragma once

#include <stdint.h>

extern "C" {
void IoOut32(uint16_t addr, uint32_t data);
uint32_t IoIn32(uint16_t addr);
uint16_t GetCS(void);
/// Interrupt Descriptor TableをCPUに登録
void LoadIDT(uint16_t limit, uint64_t offset);
/// Global Descriptor TableをCPUに登録
void LoadGDT(uint16_t limit, uint64_t offset);
/// CS（コードセグメントレジスタ）とSS（スタックセグメントレジスタ）を設定
void SetCSSS(uint16_t cs, uint16_t ss);
/// DS（データセグメントレジスタ）を設定
void SetDSAll(uint16_t value);
/// CR3レジスタを設定し、自前の階層ページング構造が利用可能となる
/// MikanOSではやらないが、再設定することで別のページング構造に切り替えることも可能
void SetCR3(uint64_t value);
uint64_t GetCR3();
/// コンテキストを切り替える
void SwitchContext(void* next_ctx, void* current_ctx);
/// コンテキストを復帰
void RestoreContext(void* ctx);
/// 指定アプリを指定の環境で呼び出す
void CallApp(int argc, char** argv, uint16_t cs, uint16_t ss, uint64_t rip, uint64_t rsp);
/// LAPICタイマ用割り込みハンドラ
void IntHandlerLAPICTimer();
/// TRレジスタを設定
void LoadTR(uint16_t sel);
/// 指定のモデル固有レジスタに値を設定
/// モデル固有レジスタ : MSR, Model Specific Register
void WriteMSR(uint32_t msr, uint64_t value);
/// syscallでコールされるOS側の関数
void SyscallEntry(void);
}

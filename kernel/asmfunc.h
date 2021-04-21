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
void SetCR3(uint64_t value);
}

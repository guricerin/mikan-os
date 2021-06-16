#pragma once

#include <cstdint>
#include <utility>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "error.hpp"
#include "graphics.hpp"

/// Unicode :
/// 文字 <-> コードポイント（数値） <-> バイト列

/// UTF-8 :
/// コードポイントの先頭1byte（の連続する1の個数）で1文字のバイト数を判断させる
/// ASCii文字のコードポイントなら、UTF-32のそれと同一

void WriteAscii(PixelWriter& writer, Vector2D<int> pos, char c, const PixelColor& color);
/// 文字列sをUTF-8で符号化された文字列と解釈して表示
void WriteString(PixelWriter& writer, Vector2D<int> pos, const char* s, const PixelColor& color);

/// UTF-8文字のバイト数を算出
int CountUTF8Size(uint8_t c);
/// UTF-9文字列から1文字取り出す
std::pair<char32_t, int> ConvertUTF8to32(const char* u8);
bool IsHankaku(char32_t c);
/// フェーズオブジェクト（字形）の準備
WithError<FT_Face> NewFTFace();
/// 与えられたコードポイントに対応する文字を描画
Error WriteUnicode(PixelWriter& writer, Vector2D<int> pos, char32_t c, const PixelColor& color);

/// 日本語フォントを初期化
void InitializeFont();

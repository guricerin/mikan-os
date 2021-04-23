#pragma once

#include <memory>
#include <vector>

#include "error.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"

/// フレームバッファはディスプレイと接続された特殊はメモリ領域
/// VRAM (Video RAM)
class FrameBuffer {
public:
    Error Initailize(const FrameBufferConfig& config);
    Error Copy(Vector2D<int> pos, const FrameBuffer& src);
    FrameBufferWriter& Writer() { return *writer_; };

private:
    FrameBufferConfig config_{};
    /// フレームバッファ本体
    std::vector<uint8_t> buffer_{};
    std::unique_ptr<FrameBufferWriter> writer_{};
};

/// 1ピクセルあたりのビット数を計算
int BitsPerPixel(PixelFormat format);
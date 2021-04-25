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
    Error Copy(Vector2D<int> dst_pos, const FrameBuffer& src, const Rectangle<int>& src_area);
    /// このウィンドウの平面領域内で、矩形領域を移動する
    void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);
    FrameBufferWriter& Writer() { return *writer_; };

private:
    FrameBufferConfig config_{};
    /// フレームバッファ本体
    std::vector<uint8_t> buffer_{};
    std::unique_ptr<FrameBufferWriter> writer_{};
};

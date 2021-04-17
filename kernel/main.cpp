#include <cstdint>

// ブートローダからフレームバッファの情報を受け取る
extern "C" void KernelMain(uint64_t frame_buffer_base, uint64_t frame_buffer_size) {
    // ピクセル描画
    uint8_t* frame_buffer = reinterpret_cast<uint8_t*>(frame_buffer_base);
    for (uint64_t i = 0; i < frame_buffer_size; i++) {
        frame_buffer[i] = i % 256;
    }

    while (1) {
        // CPU停止
        __asm__("hlt");
    }
}
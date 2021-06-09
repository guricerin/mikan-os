#include <cstdlib>
#include <random>

#include "../syscall.h"

static constexpr int kWidth = 100, kHeight = 100;

extern "C" void main(int argc, char** argv) {
    auto [layer_id, err_openwin] = SyscallOpenWindow(kWidth + 8, kHeight + 28, 10, 10, "stars");
    if (err_openwin) {
        exit(err_openwin);
    }

    // 夜空
    SyscallWinFillRectangle(LAYER_NO_REDRAW | layer_id, 4, 24, kWidth, kHeight, 0x000000);

    int num_stars = 100;
    if (argc >= 2) {
        num_stars = atoi(argv[1]);
    }

    auto [tick_start, timer_freq] = SyscallGetCurrentTick();

    std::default_random_engine rand_engine;
    // [0, kWidth - 2], [0, kHeight - 2]の範囲で乱数を生成
    std::uniform_int_distribution x_dist(0, kWidth - 2), y_dist(0, kHeight - 2);
    for (int i = 0; i < num_stars; i++) {
        int x = x_dist(rand_engine);
        int y = y_dist(rand_engine);
        // 点のような星
        SyscallWinFillRectangle(LAYER_NO_REDRAW | layer_id, 4 + x, 24 + y, 2, 2, 0xfff100);
    }
    SyscallWinRedraw(layer_id);

    auto tick_end = SyscallGetCurrentTick();
    // ms変換して表示
    printf("%d stars in %lu ms.\n", num_stars, (tick_end.value - tick_start) * 1000 / timer_freq);

    exit(0);
}
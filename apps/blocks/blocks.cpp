#include <array>
#include <bitset>
#include <cmath>
#include <cstdlib>

#include "../syscall.h"

using namespace std;

const int kNumBlocksX = 10, kNumBlocksY = 5;
const int kBlockWidth = 20, kBlockHeight = 10;
const int kBarWidth = 30, kBarHeight = 5, kBallRadius = 5;
const int kGapWidth = 30, kGapHeight = 30, kGapBar = 80, kBarFloat = 10;

const int kCanvasWidth = kNumBlocksX * kBlockWidth + 2 * kGapWidth;
const int kCanvasHeight = kGapHeight + kNumBlocksY * kBlockHeight + kGapBar + kBarHeight + kBarFloat;
const int kBarY = kCanvasHeight - kBarFloat - kBarHeight;

const int kFrameRate = 60;              // frames / sec
const int kBarSpeed = kCanvasWidth / 2; // pixels / sec
const int kBallSpeed = kBarSpeed * 0.95;

array<bitset<kNumBlocksX>, kNumBlocksY> g_blocks;

void DrawBlocks(uint64_t layer_id) {
    for (int by = 0; by < kNumBlocksY; by++) {
        const int y = 24 + kGapHeight + by * kBlockHeight;
        const uint32_t color = 0xff << (by % 3) * 8;

        for (int bx = 0; bx < kNumBlocksX; bx++) {
            if (g_blocks[by][bx]) {
                const int x = 4 + kGapWidth + bx * kBlockWidth;
                const uint32_t c = color | (0xff << ((x + by) % 3) * 8);
                SyscallWinFillRectangle(layer_id, x, y, kBlockWidth, kBlockHeight, c);
            }
        }
    }
}

void DrawBar(uint64_t layer_id, int bar_x) {
    const int x = 4 + bar_x, y = 24 + kBarY;
    SyscallWinFillRectangle(layer_id, x, y, kBarWidth, kBarHeight, 0xffffff);
}

void DrawBall(uint64_t layer_id, int x, int y) {
    SyscallWinFillRectangle(layer_id,
                            4 + x - kBallRadius, 24 + y - kBallRadius,
                            2 * kBallRadius, 2 * kBallRadius, 0x007f00);
    SyscallWinFillRectangle(layer_id,
                            4 + x - kBallRadius / 2, 24 + y - kBallRadius / 2,
                            kBallRadius, kBallRadius, 0x00ff00);
}

template <class T>
T LimitRange(const T& x, const T& min, const T& max) {
    if (x < min) {
        return min;
    } else if (x > max) {
        return max;
    } else {
        return x;
    }
}

extern "C" void main(int argc, char** arv) {
    auto [layer_id, err_openwin] = SyscallOpenWindow(kCanvasWidth + 8, kCanvasHeight + 28, 10, 10, "blocks");
    if (err_openwin) {
        exit(err_openwin);
    }

    // 全ブロック初期化
    for (int y = 0; y < kNumBlocksY; y++) {
        g_blocks[y].set();
    }

    const int kBallX = kCanvasWidth / 2 - kBallRadius - 20;
    const int kBallY = kCanvasHeight - kBarFloat - kBarHeight - kBallRadius - 20;

    int bar_x = kCanvasWidth / 2 - kBarWidth / 2;
    int ball_x = kBallX, ball_y = kBallY;
    int move_dir = 0; // -1: left, 1: right, 0: stop
    int ball_dir = 0; // degree
    int ball_dx = 0, ball_dy = 0;

    while (true) { // main loop
        // 画面全体をクリア
        SyscallWinFillRectangle(layer_id | LAYER_NO_REDRAW, 4, 24, kCanvasWidth, kCanvasHeight, 0);

        // 各オブジェクト描画
        DrawBlocks(layer_id | LAYER_NO_REDRAW);
        DrawBar(layer_id | LAYER_NO_REDRAW, bar_x);
        // ボールが画面外（上方向）にぶっとんでいない場合
        if (ball_y >= 0) {
            DrawBall(layer_id | LAYER_NO_REDRAW, ball_x, ball_y);
        }
        SyscallWinRedraw(layer_id);

        static unsigned long prev_timeout = 0;
        if (prev_timeout == 0) {
            const auto timeout = SyscallCreateTimer(TIMER_ONESHOT_REL, 1, 1000 / kFrameRate);
            prev_timeout = timeout.value;
        } else {
            prev_timeout += 1000 / kFrameRate;
            SyscallCreateTimer(TIMER_ONESHOT_ABS, 1, prev_timeout);
        }

        AppEvent events[1];
        while (true) { // event loop
            SyscallReadEvent(events, 1);
            if (events[0].type == AppEvent::kTimerTimeout) {
                break;
            } else if (events[0].type == AppEvent::kQuit) {
                goto fin;
            } else if (events[0].type == AppEvent::kKeyPush) {
                if (!events[0].arg.keypush.press) { // release
                    move_dir = 0;
                } else {
                    const auto keycode = events[0].arg.keypush.keycode;
                    if (keycode == 79) { // right arrow
                        move_dir = 1;
                    } else if (keycode == 80) { // left arrow
                        move_dir = -1;
                    } else if (keycode == 44) {            // space bar ボールを発射
                        if (ball_dir == 0 && ball_y < 0) { // game overから再開
                            ball_x = kBallX;
                            ball_y = kBallY;
                        } else if (ball_dir == 0) {
                            ball_dir = 45;
                        }
                    }

                    if (bar_x == 0 && move_dir < 0) { // バーが左端に到達
                        move_dir = 0;
                    } else if (bar_x + kBarWidth == kCanvasWidth - 1 && move_dir > 0) { // バーが右端に到達
                        move_dir = 0;
                    }
                }
            }
        } // event loop

        bar_x += move_dir * kBarSpeed / kFrameRate;
        bar_x = LimitRange(bar_x, 0, kCanvasWidth - kBarWidth - 1);

        if (ball_dir == 0) { // game over
            continue;
        }

        int ball_x_ = ball_x + ball_dx, ball_y_ = ball_y + ball_dy;
        // 壁にボールがぶつかる
        if ((ball_dx < 0 && ball_x_ < kBallRadius) || (ball_dx > 0 && kCanvasWidth - kBallRadius <= ball_x_)) {
            ball_dir = 180 - ball_dir;
        }

        if (ball_dy < 0 && ball_y_ < kBallRadius) { // 天井にボールがぶつかる
            ball_dir = -ball_dir;
        } else if (bar_x <= ball_x_ && ball_x_ < bar_x + kBarWidth &&
                   ball_dy > 0 && kBarY - kBallRadius <= ball_y_) { // バーにボールがぶつかる
            ball_dir = -ball_dir;
        } else if (ball_dy > 0 && kCanvasHeight - kBallRadius <= ball_y_) { // ボールが落下
            ball_dir = 0;
            ball_y = -1;
            continue;
        }

        do {
            if (ball_x_ < kGapWidth ||
                kCanvasWidth - kGapWidth <= ball_x_ ||
                ball_y_ < kGapHeight ||
                kGapHeight + kNumBlocksY * kBlockHeight <= ball_y_) {
                break;
            }

            const int index_x = (ball_x_ - kGapWidth) / kBlockWidth;
            const int index_y = (ball_y_ - kGapHeight) / kBlockHeight;
            // ブロックがない
            if (!g_blocks[index_y].test(index_x)) {
                break;
            }

            // ブロックがある
            g_blocks[index_y].reset(index_x);

            const int block_left = kGapWidth + index_x * kBlockWidth;
            const int block_right = kGapWidth + (index_x + 1) * kBlockWidth;
            const int block_top = kGapHeight + index_y * kBlockHeight;
            const int block_bottom = kGapHeight + (index_y + 1) * kBlockHeight;
            if ((ball_x < block_left && block_left <= ball_x_) ||
                (block_right < ball_x && ball_x_ <= block_right)) {
                ball_dir = 180 - ball_dir;
            }
            if ((ball_y < block_top && block_top <= ball_y_) ||
                (block_bottom < ball_y && ball_y_ <= block_bottom)) {
                ball_dir = -ball_dir;
            }
        } while (false);

        ball_dx = round(kBallSpeed * cos(M_PI * ball_dir / 180) / kFrameRate);
        ball_dy = round(kBallSpeed * sin(M_PI * ball_dir / 180) / kFrameRate);
        ball_x += ball_dx;
        ball_y += ball_dy;
    } // main loop

fin:
    SyscallCloseWindow(layer_id);
    exit(0);
}
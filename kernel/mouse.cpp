#include "mouse.hpp"

#include "graphics.hpp"

namespace {
    const char g_mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
        "@              ",
        "@@             ",
        "@.@            ",
        "@..@           ",
        "@...@          ",
        "@....@         ",
        "@.....@        ",
        "@......@       ",
        "@.......@      ",
        "@........@     ",
        "@.........@    ",
        "@..........@   ",
        "@...........@  ",
        "@............@ ",
        "@......@@@@@@@@",
        "@......@       ",
        "@....@@.@      ",
        "@...@ @.@      ",
        "@..@   @.@     ",
        "@.@    @.@     ",
        "@@      @.@    ",
        "@       @.@    ",
        "         @.@   ",
        "         @@@   ",
    };
} // namespace

void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position) {
    for (int dy = 0; dy < kMouseCursorHeight; dy++) {
        for (int dx = 0; dx < kMouseCursorWidth; dx++) {
            if (g_mouse_cursor_shape[dy][dx] == '@') {
                pixel_writer->Write(position.x + dx, position.y + dy, {0, 0, 0});
            } else if (g_mouse_cursor_shape[dy][dx] == '.') {
                pixel_writer->Write(position.x + dx, position.y + dy, {255, 255, 255});
            } else {
                pixel_writer->Write(position.x + dx, position.y + dy, kMouseTransparentColor);
            }
        }
    }
}

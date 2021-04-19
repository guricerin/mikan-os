#include "mouse.hpp"

#include "graphics.hpp"

namespace {
    const int g_mouse_cursor_width = 15;
    const int g_mouse_cursor_height = 24;
    const char g_mouse_cursor_shape[g_mouse_cursor_height][g_mouse_cursor_width + 1] = {
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

    void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position) {
        for (int dy = 0; dy < g_mouse_cursor_height; dy++) {
            for (int dx = 0; dx < g_mouse_cursor_width; dx++) {
                if (g_mouse_cursor_shape[dy][dx] == '@') {
                    pixel_writer->Write(200 + dx, 100 + dy, {0, 0, 0});
                } else if (g_mouse_cursor_shape[dy][dx] == '.') {
                    pixel_writer->Write(200 + dx, 100 + dy, {255, 255, 255});
                }
            }
        }
    }

    void EraseMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position, PixelColor erase_color) {
        for (int dy = 0; dy < g_mouse_cursor_height; dy++) {
            for (int dx = 0; dx < g_mouse_cursor_width; dx++) {
                if (g_mouse_cursor_shape[dy][dx] != ' ') {
                    pixel_writer->Write(position.x + dx, position.y + dy, erase_color);
                }
            }
        }
    }
} // namespace

MouseCursor::MouseCursor(PixelWriter* writer, PixelColor erase_color, Vector2D<int> initial_position) : _pixel_writer{writer}, _erase_color{erase_color}, _position{initial_position} {
    DrawMouseCursor(_pixel_writer, _position);
}
void MouseCursor::MoveRelative(Vector2D<int> displacement) {
    EraseMouseCursor(_pixel_writer, _position, _erase_color);
    _position += displacement;
    DrawMouseCursor(_pixel_writer, _position);
}
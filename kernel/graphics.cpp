#include "graphics.hpp"

void RGBResv8BitPerColorPixelWriter::Write(int x, int y, const PixelColor& color) {
    auto p = PixelAt(x, y);
    p[0] = color.r;
    p[1] = color.g;
    p[2] = color.b;
}

void BGRResv8BitPerColorPixelWriter::Write(int x, int y, const PixelColor& color) {
    auto p = PixelAt(x, y);
    p[0] = color.b;
    p[1] = color.g;
    p[2] = color.r;
}

void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos, const Vector2D<int>& size, const PixelColor& color) {
    // 上辺と下辺
    for (int dx = 0; dx < size.x; dx++) {
        writer.Write(pos.x + dx, pos.y, color);
        writer.Write(pos.x + dx, pos.y + size.y - 1, color);
    }
    // 左辺と右辺
    for (int dy = 1; dy < size.y; dy++) {
        writer.Write(pos.x, pos.y + dy, color);
        writer.Write(pos.x + size.x - 1, pos.y + dy, color);
    }
}

void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos, const Vector2D<int>& size, const PixelColor& color) {
    for (int dy = 0; dy < size.y; dy++) {
        for (int dx = 0; dx < size.x; dx++) {
            writer.Write(pos.x + dx, pos.y + dy, color);
        }
    }
}
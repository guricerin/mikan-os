#pragma once

#include "graphics.hpp"

class MouseCursor {
private:
    PixelWriter* _pixel_writer = nullptr;
    PixelColor _erase_color;
    Vector2D<int> _position;

public:
    MouseCursor(PixelWriter* writer, PixelColor erase_color, Vector2D<int> initial_position);
    void MoveRelative(Vector2D<int> displacement);
};
#pragma once

#include "graphics.hpp"

class Console {
public:
    static const int _rows = 25, _columns = 80;

    Console(PixelWriter& writer, const PixelColor& fg_color, const PixelColor& bg_color);
    void PutString(const char* s);

private:
    PixelWriter& _writer;
    const PixelColor _fg_color, _bg_color;
    char _buffer[_rows][_columns + 1]; // +1はヌル文字分
    int _cursor_row, _cursor_column;

    void NewLine();
};
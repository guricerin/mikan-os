#include "console.hpp"
#include "font.hpp"

#include <cstring>

Console::Console(PixelWriter& writer, const PixelColor& fg_color, const PixelColor& bg_color) : _writer{writer}, _fg_color{fg_color}, _bg_color{bg_color}, _buffer{}, _cursor_row{0}, _cursor_column{0} {
}

void Console::PutString(const char* s) {
    while (*s) {
        if (*s == '\n') {
            NewLine();
        } else if (_cursor_column < _columns - 1) {
            WriteAscii(_writer, 8 * _cursor_column, 16 * _cursor_row, *s, _fg_color);
            _buffer[_cursor_row][_cursor_column] = *s;
            _cursor_column++;
        }
        s++;
    }
}

void Console::NewLine() {
    _cursor_column = 0;
    if (_cursor_row < _rows - 1) {
        _cursor_row++;
    } else { // 最下行の場合
        // まずは塗りつぶす
        for (int y = 0; y < 16 * _rows; y++) {
            for (int x = 0; x < 8 * _columns; x++) {
                _writer.Write(x, y, _bg_color);
            }
        }
        // 表示領域を1行ずらす
        for (int row = 0; row < _rows - 1; row++) {
            memcpy(_buffer[row], _buffer[row + 1], _columns + 1);
            WriteString(_writer, 0, 16 * row, _buffer[row], _fg_color);
        }
        memset(_buffer[_rows - 1], 0, _columns + 1);
    }
}
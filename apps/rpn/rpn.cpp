#include <cstdlib>
#include <cstring>

#include "../../kernel/graphics.hpp"

// nm -C path/to/elf-file
// で、ファイルで使用されているシンボルとそれらが配置される仮想アドレスを列挙できる
auto& printk = *reinterpret_cast<int (*)(const char*, ...)>(0x000000000010b000);
auto& fill_rect = *reinterpret_cast<decltype(FillRectangle)*>(0x000000000010c190);
auto& scrn_writer = *reinterpret_cast<decltype(g_screen_writer)*>(0x000000000024d078);

int g_stack_ptr;
long g_stack[100];

long Pop() {
    long value = g_stack[g_stack_ptr];
    g_stack_ptr--;
    return value;
}

void Push(long value) {
    g_stack_ptr++;
    g_stack[g_stack_ptr] = value;
}

extern "C" int main(int argc, char** argv) {
    g_stack_ptr = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "+") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a + b);
            printk("[%d] <- %ld\n", g_stack_ptr, a + b);
        } else if (strcmp(argv[i], "-") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a - b);
            printk("[%d] <- %ld\n", g_stack_ptr, a - b);
        } else {
            long a = atol(argv[i]);
            Push(a);
            printk("[%d] <- %ld\n", g_stack_ptr, a);
        }
    }

    fill_rect(*scrn_writer, Vector2D<int>{100, 10}, Vector2D<int>{200, 200}, ToColor(0x00ff00));

    if (g_stack_ptr < 0) {
        return 0;
    }
    return static_cast<int>(Pop());
}
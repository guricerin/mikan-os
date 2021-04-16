extern "C" void KernelMain() {
    while (1) {
        // CPU停止
        __asm__("hlt");
    }
}
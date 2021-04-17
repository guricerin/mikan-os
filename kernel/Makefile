DEVENV_PATH = $(HOME)/osbook/devenv/x86_64-elf
CPPFLAGS = -I$(DEVENV_PATH)/include/c++/v1 -I$(DEVENV_PATH)/include -I$(DEVENV_PATH)/include/freetype2 -nostdlibinc -D__ELF__ -D_LDBL_EQ_DBL -D_GNU_SOURCE -D_POSIX_TIMERS
CXXFLAGS = -Wall -g -O2 --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17
LDFLAGS = -L$(DEVENV_PATH)/lib --entry KernelMain -z norelro --image-base 0x100000 --static

OUT_PATH = build
TARGET = $(OUT_PATH)/kernel.elf
OBJS = $(OUT_PATH)/main.o

.PHONY: default
default: $(TARGET)

.PHONY: clean
clean:
	rm -rf $(OUT_PATH)/*.o

$(TARGET): $(OBJS) Makefile
	ld.lld $(LDFLAGS) -o $(TARGET) $(OBJS)

$(OUT_PATH)/%.o: %.cpp Makefile
	mkdir -p $(OUT_PATH)
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

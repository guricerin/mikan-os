DEVENV_PATH = $(HOME)/osbook/devenv/x86_64-elf
CPPFLAGS = -I$(DEVENV_PATH)/include/c++/v1 -I$(DEVENV_PATH)/include -I$(DEVENV_PATH)/include/freetype2 -nostdlibinc -D__ELF__ -D_LDBL_EQ_DBL -D_GNU_SOURCE -D_POSIX_TIMERS
LDFLAGS = -L$(DEVENV_PATH)/lib
OUT_PATH = build

all: link

compile:
	mkdir -p $(OUT_PATH)
	clang++ $(CPPFLAGS) -Wall -g -O2 --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17 -c main.cpp -o $(OUT_PATH)/main.o

link: compile
	ld.lld $(LDFLAGS) --entry KernelMain -z norelro --image-base 0x100000 --static -o $(OUT_PATH)/kernel.elf $(OUT_PATH)/main.o

.PHONY: clean
clean:
	rm -rf ./$(OUT_PATH)

DEVENV_PATH = $(HOME)/osbook/devenv/x86_64-elf
CPPFLAGS = -I$(DEVENV_PATH)/include/c++/v1 -I$(DEVENV_PATH)/include -I$(DEVENV_PATH)/include/freetype2 -nostdlibinc -D__ELF__ -D_LDBL_EQ_DBL -D_GNU_SOURCE -D_POSIX_TIMERS
CXXFLAGS = -Wall -g -O2 --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17
CFLAGS = -Wall -g -O2 --target=x86_64-elf -ffreestanding -mno-red-zone 
LDFLAGS = -L$(DEVENV_PATH)/lib --entry KernelMain -z norelro --image-base 0x100000 --static

TARGET = kernel.elf
OBJS = main.o graphics.o font.o hankaku.o newlib_support.o console.o

.PHONY: default
default: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o *.bin

$(TARGET): $(OBJS) Makefile
	ld.lld $(LDFLAGS) -o $(TARGET) $(OBJS) -lc

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

%.o: %.c Makefile
	clang $(CPPFLAGS) $(CFLAGS) -c $< -o $@

hankaku.bin: hankaku.txt
	../scripts/makefont.py -o $@ $<

hankaku.o: hankaku.bin
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $< $@

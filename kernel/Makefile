OUT_PATH = build

all: link

compile:
	mkdir -p $(OUT_PATH)
	clang++ -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17 -c main.cpp -o $(OUT_PATH)/main.o

link: compile
	ld.lld --entry KernelMain -z norelro --image-base 0x100000 --static -o $(OUT_PATH)/kernel.elf $(OUT_PATH)/main.o

.PHONY: clean
clean:
	rm -rf ./$(OUT_PATH)

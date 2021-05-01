TARGET = onlyhlt

.PHONY: all
all: $(TARGET)

$(TARGET): onlyhlt.asm Makefile
	# フラットバイナリとして生成
	nasm -f bin -o $@ $<

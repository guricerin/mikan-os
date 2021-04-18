SHELL=/bin/bash
SCRIPT_ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

KERNEL_PATH = $(SCRIPT_ROOT)/kernel
LOADER_PATH = $(HOME)/edk2

.PHONY: default
default: kernel loader

.PHONY: kernel
kernel:
	$(MAKE) -C $(KERNEL_PATH)

.PHONY: loader
loader:
	cd $(LOADER_PATH) && source ./edksetup.sh && build

.PHONY: run
run: default
	./scripts/run-qemu.sh

.PHONY: setup
setup:
	./scripts/setup.sh
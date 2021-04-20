SHELL=/bin/bash
SCRIPT_ROOT = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

KERNEL_PATH = $(SCRIPT_ROOT)/kernel
LOADER_PATH = $(HOME)/edk2
OSBOOK_DEVENV_PATH = $(HOME)/osbook/devenv

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
	$(OSBOOK_DEVENV_PATH)/run_qemu.sh $(LOADER_PATH)/Build/MikanLoaderX64/DEBUG_CLANG38/X64/Loader.efi $(KERNEL_PATH)/kernel.elf

.PHONY: setup
setup:
	./scripts/setup.sh
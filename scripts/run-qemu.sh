#!/bin/bash
set -eux

readonly EDK2_PATH=$HOME/edk2
readonly OSBOOK_DEVENV_PATH=$HOME/osbook/devenv
readonly SCRIPT_ROOT=$(cd $(dirname ${0}); pwd)
readonly KERNEL_PATH=$SCRIPT_ROOT/../kernel/build/kernel.elf

# 実行する前に以下のビルド作業を行うこと
# $ cd $EDK2_PATH 
# $ source edksetup.sh
# $ build

# run qemu
$OSBOOK_DEVENV_PATH/run_qemu.sh $EDK2_PATH/Build/MikanLoaderX64/DEBUG_CLANG38/X64/Loader.efi $KERNEL_PATH
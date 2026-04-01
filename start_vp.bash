#!/bin/bash
# Get directory of script
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do
    DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="${DIR}/$SOURCE"
done
MAIN_DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"

export M5_PATH=${MAIN_DIR}/images/system

./gem5/build/ARM/gem5.opt ${MAIN_DIR}/gem5_configs/aarch64_arm_board.py \
    --kernel ${M5_PATH}/binaries/vmlinux \
    --disk-image ${M5_PATH}/binaries/rootfs.ext2 \
    --dtb ${M5_PATH}/system.dtb \
    --root-device /dev/vda

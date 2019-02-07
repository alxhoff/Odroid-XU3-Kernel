#!/bin/sh

ROOT_DIR="/home/foo/Android7.1/kernel/hardkernel/odroidxu3"

if [ $# -eq 0 ]
then
    echo "Please specify board (odroidxu3 ;) )"
    exit 0
fi

PRODUCT_BOARD=$1

adb $ANDROID_DEVICE reboot fastboot

fastboot flash kernel $ROOT_DIR/arch/arm/boot/zImage-dtb

fastboot reboot

$ROOT_DIR/install_modules.sh "$ANDROID_DEVICE"

fastboot reboot

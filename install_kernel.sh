#!/bin/sh

KERNEL_DIR=.

./install_modules.sh

adb $ANDROID_DEVICE reboot fastboot

fastboot flash kernel $KERNEL_DIR/arch/arm/boot/zImage-dtb

fastboot reboot

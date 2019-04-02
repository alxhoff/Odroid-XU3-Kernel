#!/bin/sh

./install_modules.sh

adb reboot fastboot

fastboot flash kernel arch/arm/boot/zImage-dtb

fastboot reboot


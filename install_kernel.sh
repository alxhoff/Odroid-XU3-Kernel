#!/bin/sh

KERNEL_DIR=.

while [ "$1" != "" ]; do
    case $1 in
		-s | --device)  		shift
								ANDROID_DEVICE="-s $1"
								;;
		-b | --build-kernel)	BUILD=1
								;;
        * )  					;;
    esac
    shift
done

if [ ! -z $BUILD ]; then
	./build_rcs.sh odroidxu3 kernel
fi

./install_modules.sh

adb $ANDROID_DEVICE reboot fastboot

fastboot flash kernel $KERNEL_DIR/arch/arm/boot/zImage-dtb

fastboot reboot

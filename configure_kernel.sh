#!/bin/bash

CPU_JOB_NUM=$(nproc --all)

KERNEL_CROSS_COMPILE_PATH="arm-linux-androideabi-"
echo "cross compiler path: $KERNEL_CROSS_COMPILE_PATH"


function check_exit()
{
    if [ $? != 0 ]
    then
        exit $?
    fi
}

function build_kernel()
{
    echo "make ARCH=arm CROSS_COMPILE=$KERNEL_CROSS_COMPILE_PATH ANDROID_MAJOR_VERSION=7 ANDROID_VERSION=1 menuconfig"
    echo
    make ARCH=arm CROSS_COMPILE=$KERNEL_CROSS_COMPILE_PATH ANDROID_MAJOR_VERSION=7 ANDROID_VERSION=1 menuconfig
    check_exit
}

build_kernel

echo success!!

exit 0

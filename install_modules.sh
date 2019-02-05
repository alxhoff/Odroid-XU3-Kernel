# installs driver libraries on device
DEST_DIR=/system/lib/modules/
SRC_DIR=$(pwd)/../../../out/target/product/odroidxu3/system/lib/modules
ADB=adb

if [ "$#" -eq 1 ]; then
	OPTIONAL_ANDROID_ID="$1"
fi

$ADB $OPTIONAL_ANDROID_ID root
$ADB $OPTIONAL_ANDROID_ID wait-for-device
$ADB $OPTIONAL_ANDROID_ID shell mount -o rw,remount /system
$ADB $OPTIONAL_ANDROID_ID shell chmod 777 $DEST_DIR
$ADB $OPTIONAL_ANDROID_ID push $SRC_DIR/* $DEST_DIR

# installs driver libraries on device
DEST_DIR=/system/lib/modules
SRC_DIR=./
ADB=adb
$ADB root
$ADB shell mount -o rw,remount /system
$ADB shell chmod 777 /system/lib
$ADB push $SRC_DIR/sound/usb/snd-usbmidi-lib.ko $DEST_DIR
$ADB push $SRC_DIR/sound/usb/snd-usb-audio.ko $DEST_DIR
# activates ethernet
$ADB push $SRC_DIR/drivers/net/usb/smsc95xx.ko $DEST_DIR
$ADB push $SRC_DIR/drivers/net/usb/usbnet.ko $DEST_DIR
$ADB push $SRC_DIR/drivers/net/usb/r8152.ko $DEST_DIR
$ADB push $SRC_DIR/drivers/net/usb/cdc_ether.ko $DEST_DIR
$ADB push $SRC_DIR/drivers/net/usb/ax88179_178a.ko $DEST_DIR
$ADB push $SRC_DIR/drivers/8192cu/rtl8xxx_CU/8192cu.ko $DEST_DIR

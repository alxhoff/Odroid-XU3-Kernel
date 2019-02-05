# installs driver libraries on device
ADB=adb

if [ "$#" -eq 1 ]; then
	OPTIONAL_ANDROID_ID="$1"
fi

$ADB $OPTIONAL_ANDROID_ID root
$ADB $OPTIONAL_ANDROID_ID shell ip link set down dev eth0
$ADB $OPTIONAL_ANDROID_ID shell ip link set dev eth0 address 02:b1:fd:f0:48:76
$ADB $OPTIONAL_ANDROID_ID shell ip link set up dev eth0


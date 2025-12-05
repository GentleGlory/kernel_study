#!/bin/bash

CONFIGFS="/sys/kernel/config"
GADGET="$CONFIGFS/usb_gadget/g1"
FUNCTION="$GADGET/functions/uvc.0"

VID="0x1d6b" # Linux Foundation 
PID="0x0102" # Multifunction Composite Gadget 
MANUFACTURER="MyCompany" 
PRODUCT="Dual Format UVC Camera" 
SERIAL="001"

mkdir -p $GADGET
mkdir -p $FUNCTION

cd $GADGET
echo $VID > idVendor 
echo $PID > idProduct 
echo 0x0100 > bcdDevice # v1.0.0 
echo 0x0200 > bcdUSB # USB 2.0

mkdir -p strings/0x409 
echo $SERIAL > strings/0x409/serialnumber 
echo $MANUFACTURER > strings/0x409/manufacturer 
echo $PRODUCT > strings/0x409/product

mkdir -p $GADGET/configs/c.1 
mkdir -p $GADGET/configs/c.1/strings/0x409 
echo "UVC Config" > $GADGET/configs/c.1/strings/0x409/configuration 
echo 500 > $GADGET/configs/c.1/MaxPower

echo 16 > $FUNCTION/uvc_num_request
echo 3072 > $FUNCTION/streaming_maxpacket
echo 3 > $FUNCTION/streaming_maxburst
echo 1 > $FUNCTION/streaming_interval

create_frame() {
		# Example usage:
		# create_frame <width> <height> <group> <format name>

		WIDTH=$1
		HEIGHT=$2
		FORMAT=$3
		NAME=$4

		wdir=$FUNCTION/streaming/$FORMAT/$NAME/${HEIGHT}p

		mkdir -p $wdir
		echo $WIDTH > $wdir/wWidth
		echo $HEIGHT > $wdir/wHeight
		echo $(( $WIDTH * $HEIGHT * 2 )) > $wdir/dwMaxVideoFrameBufferSize
		cat <<EOF > $wdir/dwFrameInterval
333333
666666
500000
1000000
EOF
}

create_frame 1280 720 mjpeg mjpeg
create_frame 1920 1080 mjpeg mjpeg
create_frame 1280 720 uncompressed yuyv
create_frame 1920 1080 uncompressed yuyv


mkdir $FUNCTION/streaming/header/h

# This section links the format descriptors and their associated frames
# to the header
cd $FUNCTION/streaming/header/h
ln -s ../../uncompressed/yuyv
ln -s ../../mjpeg/mjpeg

# This section ensures that the header will be transmitted for each
# speed's set of descriptors. If support for a particular speed is not
# needed then it can be skipped here.
cd ../../class/fs
ln -s ../../header/h
cd ../../class/hs
ln -s ../../header/h
cd ../../class/ss
ln -s ../../header/h
cd ../../../control
mkdir header/h
ln -s header/h class/fs
ln -s header/h class/ss

cd $GADGET

ln -s $FUNCTION $GADGET/configs/c.1

UDC_NAME=$(ls /sys/class/udc | head -n 1) 
if [ -z "$UDC_NAME" ]; then 
	echo "Error: No UDC device found." 
	exit 1 
fi 
echo $UDC_NAME > UDC 
echo "UVC Gadget created successfully on $UDC_NAME!"
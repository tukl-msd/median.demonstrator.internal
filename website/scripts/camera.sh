#/bin/bash

# sh scriptname 1

if [ $# -ne 1 ]
then
    echo "ERROR: # Arguments not equal to 1 ($#)."
else
	echo "$1" > /sys/devices/platform/vio/pi_cam
fi

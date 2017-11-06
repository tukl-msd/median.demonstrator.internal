#/bin/bash

# sh scriptname 1

if [ $# -ne 1 ]
then
    echo "ERROR: # Arguments not equal to 1 ($#)."
else
	rmdir /config/device-tree/overlays/vio/ > /dev/null
	rmdir /config/device-tree/overlays/median/ > /dev/null

    echo "$1" > /sys/class/fpga_manager/fpga0/firmware
    
    if [ $(cat /sys/class/fpga_manager/fpga0/state) = "operating" ]
	then
		mkdir /config/device-tree/overlays/vio/ > /dev/null
		mkdir /config/device-tree/overlays/median/ > /dev/null

		cp /home/median.demonstrator.internal/dt-overlay/median-system/median-system.dtbo /config/device-tree/overlays/median/dtbo
		cp /home/median.demonstrator.internal/dt-overlay/vio/vio.dtbo /config/device-tree/overlays/vio/dtbo
		
		echo "operating"
	fi
fi

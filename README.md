# median.demonstrator.internal
Content of the Zynqberry. 

## Installation

### Clone the repository

    cd /home
    git clone https://github.com/tukl-msd/median.demonstrator.internal.git
    
### Create a link for lighttpd

    rm -r /var/www
    ln -s /home/median.demonstrator.internal/website /var/www
    
### Register service _system_

    /home/median.demonstrator.internal/service/setup.sh
    
### Create links for all drivers

    ln -s /home/median.demonstrator.internal/driver/fclk/fclk.ko /lib/modules/$(uname -r)/
    ln -s /home/median.demonstrator.internal/driver/stpg/stpg.ko /lib/modules/$(uname -r)/
    ln -s /home/median.demonstrator.internal/driver/pi-cam/pi-cam.ko /lib/modules/$(uname -r)/
    ln -s /home/median.demonstrator.internal/driver/vio/vio.ko /lib/modules/$(uname -r)/
    
### Update module dependencies

    depmod -a

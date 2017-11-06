obj-m += vio.o
 
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) KBUILD_EXTRA_SYMBOLS="$(PWD)/../stpg/Module.symvers $(PWD)/../pi-cam/Module.symvers" modules
 
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

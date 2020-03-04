obj-m := fiber.o
fiber-objs := fiber_ioctl.o

KDIR ?= /lib/modules/$(shell uname -r)/build
 
module:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean


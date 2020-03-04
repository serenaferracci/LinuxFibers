obj-m := fiber.o
fiber-objs := fiber_ioctl.o proc.o

KDIR ?= /lib/modules/$(shell uname -r)/build
 
module:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# Otherwise KERNELRELEASE is defined; we've been invoked from the
# kernel build system and can use its language.


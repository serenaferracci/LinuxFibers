CONFIG_MODULE_SIG=n
CONFIG_MODULE_SIG_ALL=n

# If called directly from the command line, invoke the kernel build system.
ifeq ($(KERNELRELEASE),)
 
    KERNEL_SOURCE := /home/serenaferracci/Documents/Kernel-14.16/linux-4.16
    PWD := $(shell pwd)
default: module
 
module:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules
 
clean:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean
 
# Otherwise KERNELRELEASE is defined; we've been invoked from the
# kernel build system and can use its language.
else
 
	obj-m := fiber.o
	fiber-objs := fiber_ioctl.o proc.o
 
endif

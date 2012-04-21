ifneq ($(KERNELRELEASE),)
	obj-m := ycsysfs.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	@rm -fr *.o *.ko *.mod.* modules.* Module.* .*.cmd .tmp_versions

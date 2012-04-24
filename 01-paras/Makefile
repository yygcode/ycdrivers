ifneq ($(KERNELRELEASE),)
	ycparas-objs := paras.o
	obj-m := ycparas.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	@rm -fr *.o *.ko *.mod.* modules.* Module.* .*.cmd .tmp_versions

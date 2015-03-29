EXTRA_CFLAGS +=
APP_EXTRA_FLAGS:= -O2 -ansi -pedantic
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(PWD)
GCC:=gcc
RM:=rm

.PHONY : clean

all: clean modules work monitor

obj-m:= mp3.o

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR) modules

work: work.c
	$(GCC) -g -o work work.c

monitor: monitor.c
	$(GCC) -g -o monitor monitor.c	

clean:
	$(RM) -f *~ *.ko *.o *.mod.c Module.symvers modules.order

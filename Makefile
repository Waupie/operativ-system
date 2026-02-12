obj-m += my_module.o
my_module-objs := src/main_module.o src/hashtable_module.o src/proc_handlers.o src/daemon_signal.o src/cmd_history.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: modules daemon

modules:
	make -C $(KDIR) M=$(PWD) modules

daemon: src/daemon.c
	gcc -Wall -O2 -o daemon src/daemon.c

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f daemon

obj-m += my_module.o
my_module-objs := src/kernel/main_module.o src/kernel/hashtable_module.o src/kernel/net_kvstore.o src/kernel/daemon_module.o src/kernel/kvstore.o tests/test_hashtable.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: modules daemon

modules:
	make -C $(KDIR) M=$(PWD) modules

daemon: src/user/daemon.c
	gcc -Wall -O2 -o daemon src/user/daemon.c

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f daemon

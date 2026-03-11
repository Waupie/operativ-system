obj-m += my_module.o
my_module-objs := src/kernel/main_module.o src/kernel/hashtable_module.o src/kernel/daemon_module.o src/kernel/kvstore.o tests/test_hashtable.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

DAEMON_SRC := $(wildcard src/user/*.c)

all: modules daemon

modules:
	make -C $(KDIR) M=$(PWD) modules

daemon: $(DAEMON_SRC)
	gcc -Wall -O2 -pthread -o daemon $(DAEMON_SRC)

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f daemon

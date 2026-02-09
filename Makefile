
obj-m += my_module.o
my_module-objs := src/main_module.o src/hashtable_module.o tests/test_hashtable.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
CUR_MAKEFILE_PATH = $(abspath $(lastword $(MAKEFILE_LIST)))
ROOT_PATH ?= $(patsubst %/, %, $(dir $(CUR_MAKEFILE_PATH)))/../../

CFLAGS = -Wall -Werror -fPIC -I$(ROOT_PATH)/include -I$(ROOT_PATH)/include/vessel -I$(ROOT_PATH)/src/inc -Wall -D$(CONFIG_NR_CPUS) -D$(CONFIG_NODES_SHIFT) -D_GNU_SOURCE -mrdpid -mfsgsbase -DVESSEL_UIPI -muintr
LDFLAGS = -lpthread -lrt -ldl 
ifeq ($(CONFIG_DEBUG),y)
CFLAGS += -DDEBUG -DCCAN_LIST_DEBUG -rdynamic -O0 -ggdb -mssse3 -g
LDFLAGS += -rdynamic
else
CFLAGS += -DNDEBUG -O3 -mssse3
endif

interface_src  = $(wildcard ./*.c)
interface_obj = $(interface_src:.c=.o)
interface_dep = $(interface_obj:.o=.d)

ifneq ( $(MAKECMDGOALS),clean)
-include $(interface_dep)
endif

libveinterface.so: $(interface_src)
	$(CC) -shared -fPIC $^ -o $@  $(CFLAGS) $(LDFLAGS)

clean:
	rm -f libveinterface.so

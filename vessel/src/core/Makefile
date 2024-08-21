CUR_MAKEFILE_PATH = $(abspath $(lastword $(MAKEFILE_LIST)))
ROOT_PATH ?= $(patsubst %/, %, $(dir $(CUR_MAKEFILE_PATH)))/../../

CFLAGS = -Wall -Werror -I$(ROOT_PATH)/include -I$(ROOT_PATH)/include/vessel -I$(ROOT_PATH)/src/inc -Wall -D$(CONFIG_NR_CPUS) -D$(CONFIG_NODES_SHIFT) -D_GNU_SOURCE -mrdpid -mfsgsbase -muintr
LDFLAGS = -lpthread -lrt -ldl 
ifeq ($(CONFIG_DEBUG),y)
CFLAGS  += -DDEBUG -DCCAN_LIST_DEBUG -rdynamic -O0 -ggdb -mssse3 -g
LDFLAGS += -rdynamic
else
CFLAGS += -DNDEBUG -O3 -mssse3
endif

all: libvesselcore.so

src  = $(wildcard $(ROOT_PATH)/src/base/*.c)
src += $(wildcard $(ROOT_PATH)/src/core/*.c)
src += $(wildcard $(ROOT_PATH)/src/mpk/*.c)
src += $(wildcard $(ROOT_PATH)/src/uexec/*.c)
src += $(wildcard $(ROOT_PATH)/src/apis/*.c)
all_asm = $(wildcard $(ROOT_PATH)/src/asm/*.S)
asm = $(filter-out  %switch_to.S %uipi_entry.S, $(all_asm))

ifeq ($(CONFIG_UIPI),y)
CFLAGS += -DVESSEL_UIPI
asm += $(ROOT_PATH)/src/asm/switch_to.S
asm += $(ROOT_PATH)/src/asm/uipi_entry.S
endif


dep = $(src:.c=.d)

ifneq ( $(MAKECMDGOALS),clean)
-include $(dep)
endif

libvesselcore.so: $(src) $(asm)
	$(CC) -shared -fPIC $^ -o $@ $(LDFLAGS) $(CFLAGS) $(BASELDFLAGS)  -lrt

clean:
	rm -f libvesselcore.so

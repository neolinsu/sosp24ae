CUR_MAKEFILE_PATH = $(abspath $(lastword $(MAKEFILE_LIST)))
ROOT_PATH ?= $(patsubst %/, %, $(dir $(CUR_MAKEFILE_PATH)))/../../

include $(ROOT_PATH)/scripts/share.mk

CFLAGS = -Wall -Werror -fPIC -ftls-model=local-exec -I$(ROOT_PATH)/include -I$(ROOT_PATH)/include/vessel -I$(ROOT_PATH)/src/inc -Wall -D$(CONFIG_NR_CPUS) -D$(CONFIG_NODES_SHIFT) -D_GNU_SOURCE -mrdpid -mfsgsbase -muintr
LDFLAGS = -lpthread -lrt -ldl

ifeq ($(CONFIG_DEBUG),y)
CFLAGS += -DDEBUG -DCCAN_LIST_DEBUG -rdynamic -O0 -ggdb -mssse3 -g
LDFLAGS += -rdynamic
else
CFLAGS += -DNDEBUG -O3 -mssse3
endif

ifeq ($(CONFIG_DIRECTPATH),y)
CFLAGS += -DDIRECTPATH $(MLX5_INC)
LDFLAGS += $(MLX5_LIBS)
endif

net_src = $(wildcard $(ROOT_PATH)/src/runtime/net/*.c)
cxl_src = $(wildcard $(ROOT_PATH)/src/runtime/cxl/*.c)

ifeq ($(CONFIG_CXLTP), y)
CFLAGS += -DVESSEL_CXLTP
endif
runtime_src  = $(wildcard $(ROOT_PATH)/src/base/*.c) $(wildcard $(ROOT_PATH)/src/runtime/*.c) $(cxl_src) $(net_src)
runtime_src += $(wildcard $(ROOT_PATH)/src/runtime/net/directpath/*.c)
runtime_src += $(wildcard $(ROOT_PATH)/src/runtime/net/directpath/mlx5/*.c)
runtime_src += $(wildcard $(ROOT_PATH)/src/runtime/mmem/*.c)
runtime_src += $(wildcard $(ROOT_PATH)/src/runtime/tls/*.c)
runtime_src += $(wildcard $(ROOT_PATH)/src/runtime/rpc/*.c)
runtime_asm  = $(wildcard $(ROOT_PATH)/src/runtime/*.S)
runtime_obj = $(runtime_src:.c=.o)
runtime_obj += $(runtime_asm:.S=.o)
runtime_dep = $(runtime_obj:.o=.d)

ifeq ($(CONFIG_UIPI),y)
CFLAGS += -DVESSEL_UIPI
endif

all: libveruntime.a

%.d: %.c
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

%.d: %.S
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

%.o: %.S
	$(CC)  -c $< -o $@ $(CFLAGS) 

ifneq ( $(MAKECMDGOALS),clean)
-include $(runtime_dep)
endif

libveruntime.a: $(runtime_obj) $(runtime_dep)
	$(AR) rcs $@ $^

clean:
	rm -f libveruntime.a $(runtime_obj) $(runtime_dep)
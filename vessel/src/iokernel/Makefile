include $(ROOT_PATH)/scripts/share.mk

CFLAGS = -Wall -Werror -fPIC -I$(ROOT_PATH)/include -I$(ROOT_PATH)/include/vessel -I$(ROOT_PATH)/src/inc -Wall -D$(CONFIG_NR_CPUS) -D$(CONFIG_NODES_SHIFT) -D_GNU_SOURCE -mrdpid -mfsgsbase -muintr #-DMY_STATS
LDFLAGS = -lpthread -lrt -ldl

ifeq ($(CONFIG_DEBUG),y)
CFLAGS += -DDEBUG -DCCAN_LIST_DEBUG -rdynamic -O0 -ggdb -mssse3 -g
LDFLAGS += -rdynamic
else
CFLAGS += -DNDEBUG -O3 -mssse3
endif

ifeq ($(CONFIG_UIPI),y)
CFLAGS += -DVESSEL_UIPI
endif

ifeq ($(CONFIG_MLX5),y)
CFLAGS += -DMLX5 $(MLX5_INC)
LDFLAGS += $(MLX5_LIBS)
endif

ifeq ($(CONFIG_SERVER_KEEP_STEAL),y)
CFLAGS += -DSERVER_KEEP_STEAL
endif

DPDK_PATH = $(ROOT_PATH)/third_party/dpdk

iokernel_src = $(wildcard $(ROOT_PATH)/src/iokernel/*.c)
iokernel_src += $(wildcard $(ROOT_PATH)/src/iokernel/utils/*.c)
iokernel_src += $(wildcard $(ROOT_PATH)/src/base/*.c)
iokernel_obj = $(iokernel_src:.c=.o)
$(iokernel_obj): CFLAGS += -I$(DPDK_PATH)/build/include

PCM_DEPS_PATH = $(ROOT_PATH)/third_party/pcm/build/src
PCM_DEPS = $(PCM_DEPS_PATH)/libpcm.a
PCM_LIBS = -lm -lstdc++

DPDK_LIBS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs --static libdpdk)

all: veiokerneld


%.d: %.c
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

%.d: %.S
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

%.o: %.S
	$(CC)  -c $< -o $@ $(CFLAGS) 

veiokerneld: $(iokernel_obj)  $(PCM_DEPS)
	$(LD) -o $@ $(iokernel_obj) $(LDFLAGS) $(DPDK_LIBS) \
	$(PCM_DEPS) $(PCM_LIBS) -lpthread -lnuma -ldl

clean:
	rm -f veiokerneld $(iokernel_obj)
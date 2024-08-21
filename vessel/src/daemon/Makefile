CFLAGS = -Wall -Werror -I$(ROOT_PATH)/include -I$(ROOT_PATH)/include/vessel -I$(ROOT_PATH)/src/inc -Wall -D$(CONFIG_NR_CPUS) -D$(CONFIG_NODES_SHIFT) -D_GNU_SOURCE -mrdpid -mfsgsbase -DVESSEL_UIPI  -muintr
LDFLAGS = -lpthread -lrt -ldl 


vessel_daemon_src = $(ROOT_PATH)/src/daemon/daemon.c
vessel_daemon_obj = $(vessel_daemon_src:.c=.o)
vessel_daemon_dep = $(vessel_daemon_obj:.o=.d)

vessel_cli_src = $(ROOT_PATH)/src/daemon/client.c
vessel_cli_obj = $(vessel_cli_src:.c=.o)
vessel_cli_dep = $(vessel_cli_obj:.o=.d)

all: vessel vessel-cli

ifneq ( $(MAKECMDGOALS),clean)
-include $(vessel_daemon_dep) $(vessel_cli_dep)
endif

vessel: $(vessel_daemon_src) $(ROOT_PATH)/src/core/libvesselcore.so
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)  -L$(ROOT_PATH)/src/core -lvesselcore

vessel-cli: $(vessel_cli_src) $(ROOT_PATH)/src/core/libvesselcore.so
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -L$(ROOT_PATH)/src/core -lvesselcore

.PHONY: clean
clean:
	rm -f vessel vessel-cli
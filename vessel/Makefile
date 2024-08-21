export CHECKFLAGS = -D__CHECKER__ -Waddress-space

export CC   = gcc
export LD   = gcc
export AR   = ar
export MAKE = make

CUR_MAKEFILE_PATH =$(abspath $(lastword $(MAKEFILE_LIST)))
export ROOT_PATH =$(patsubst %/,%,$(dir $(CUR_MAKEFILE_PATH)))
export prefix ?=$(ROOT_PATH)

include $(ROOT_PATH)/.config


.PHONY: all
all: veinterface vesselcore veruntime veiokerneld veshim vessel
.PHONY: veinterface vesselcore veruntime veiokerneld veshim vessel

veinterface:
	$(MAKE) -C ./src/interface

vesselcore:
	$(MAKE) -C ./src/core

veruntime:
	$(MAKE) -C ./src/runtime

veiokerneld:
	$(MAKE) -C ./src/iokernel

veshim:
	$(MAKE) -C ./src/reentrant_shim
	$(MAKE) -C ./src/simple_shim
	$(MAKE) -C ./bindings/cc

vessel: vesselcore
	$(MAKE) -C ./src/daemon

include_files_root = $(wildcard $(ROOT_PATH)/include/vessel/*.h)
include_files_asm  += $(wildcard $(ROOT_PATH)/src/inc/asm/*.h)
include_files_base += $(wildcard $(ROOT_PATH)/src/inc/base/*.h)
include_files_iokernel += $(wildcard $(ROOT_PATH)/src/inc/iokernel/*.h)
include_files_vcontext += $(wildcard $(ROOT_PATH)/src/inc/vcontext/*.h)
include_files_net += $(wildcard $(ROOT_PATH)/src/inc/net/*.h)
include_files_runtime += $(wildcard $(ROOT_PATH)/include/vessel/runtime/*.h)
include_files_runtime_cxl += $(wildcard $(ROOT_PATH)/include/vessel/runtime/cxl/*.h)
include_files_runtime_mmem += $(wildcard $(ROOT_PATH)/include/vessel/runtime/mmem/*.h)
include_files_runtime_tls += $(wildcard $(ROOT_PATH)/include/vessel/runtime/tls/*.h)

ifeq ($(MAKECMDGOALS), install)
ifeq ($(prefix), $(ROOT_PATH))
$(warning Can not install in root path, set to $(ROOT_PATH)/dists)
prefix:=$(ROOT_PATH)/dist
endif
endif

ifeq ($(MAKECMDGOALS), uninstall)
ifeq ($(prefix), $(ROOT_PATH))
$(warning Can not install in root path, set to $(ROOT_PATH)/dists)
prefix:=$(ROOT_PATH)/dist
endif
endif

third_party_libs = $(ROOT_PATH)/third_party_libs/libvjemalloc.so.2
third_party_libs += $(ROOT_PATH)/third_party_libs/ld-linux-x86-64-vessel.so.2
rdma_core_builds = $(ROOT_PATH)/third_party/rdma-core/build/*

.PHONY: uninstall
uninstall:
	rm -rf $(prefix)/include/vessel
	rm -f $(prefix)/lib64/libveinterface.so 
	rm -f $(prefix)/lib64/libvesselcore.so 
	rm -f $(prefix)/lib64/libveruntime.a
	rm -rf $(prefix)/lib64/vessel
	rm -f $(prefix)/bin/iokernel/veiokerneld
	rm -f $(prefix)/lib64/libreshim.a
	rm -f $(prefix)/lib64/libshim.a
	rm -f $(prefix)/bin/vessel
	rm -f $(prefix)/bin/vessel-cli
	rm -rf $(prefix)/share/vessel

.PHONY: install
install: uninstall
	mkdir -p $(prefix)/lib64
	mkdir -p $(prefix)/lib64/vessel/runtime
	mkdir -p $(prefix)/lib64/vessel/rdma-core
	mkdir -p $(prefix)/include/vessel
	mkdir -p $(prefix)/include/vessel/asm
	mkdir -p $(prefix)/include/vessel/base
	mkdir -p $(prefix)/include/vessel/iokernel
	mkdir -p $(prefix)/include/vessel/vcontext
	mkdir -p $(prefix)/include/vessel/net
	mkdir -p $(prefix)/include/vessel/runtime
	mkdir -p $(prefix)/include/vessel/runtime/cxl
	mkdir -p $(prefix)/include/vessel/runtime/mmem
	mkdir -p $(prefix)/include/vessel/runtime/tls
	mkdir -p $(prefix)/bin
	mkdir -p $(prefix)/share/vessel
	install $(include_files_root) -t $(prefix)/include/vessel
	install $(include_files_asm) -t $(prefix)/include/vessel/asm
	install $(include_files_base) -t $(prefix)/include/vessel/base
	install $(include_files_iokernel) -t $(prefix)/include/vessel/iokernel
	install $(include_files_vcontext) -t $(prefix)/include/vessel/vcontext
	install $(include_files_net) -t $(prefix)/include/vessel/net
	install $(include_files_runtime) -t $(prefix)/include/vessel/runtime
	install $(include_files_runtime_cxl) -t $(prefix)/include/vessel/runtime/cxl
	install $(include_files_runtime_mmem) -t $(prefix)/include/vessel/runtime/mmem
	install $(include_files_runtime_tls) -t $(prefix)/include/vessel/runtime/tls
	install $(third_party_libs) -t $(prefix)/lib64/vessel/runtime
	cp -r $(rdma_core_builds)  $(prefix)/lib64/vessel/rdma-core/
	install $(ROOT_PATH)/src/interface/libveinterface.so $(prefix)/lib64
	install $(ROOT_PATH)/src/core/libvesselcore.so $(prefix)/lib64
	install $(ROOT_PATH)/src/runtime/libveruntime.a $(prefix)/lib64
	install $(ROOT_PATH)/src/iokernel/veiokerneld $(prefix)/bin
	install $(ROOT_PATH)/src/reentrant_shim/libreshim.a $(prefix)/lib64
	install $(ROOT_PATH)/src/simple_shim/libshim.a $(prefix)/lib64
	install $(ROOT_PATH)/src/daemon/vessel $(prefix)/bin
	install $(ROOT_PATH)/src/daemon/vessel-cli $(prefix)/bin
	install $(ROOT_PATH)/src/runtime/tls/tls.ld $(prefix)/include/vessel/runtime/tls
	install $(ROOT_PATH)/Makefile $(prefix)/share/vessel/
	install $(ROOT_PATH)/.config $(prefix)/share/vessel/
	install $(ROOT_PATH)/src/runtime/tls/tls.ld $(prefix)/share/vessel/
	sed -i '/^export prefix=.*/c\export prefix=$(prefix)' .config

.PHONY: clean
clean:
	$(MAKE) -C ./src/interface clean
	$(MAKE) -C ./src/core clean
	$(MAKE) -C ./src/runtime clean
	$(MAKE) -C ./src/iokernel clean
	$(MAKE) -C ./src/reentrant_shim clean
	$(MAKE) -C ./src/simple_shim clean
	$(MAKE) -C ./src/daemon clean
RUNTIME_INC = -I$(prefix)/include/vessel

RUNTIME_LIBS = -lpthread  -lshim -lveruntime -lveinterface
RUNTIME_LIBS += -L$(prefix)/lib64 -ldl

RUNTIME_LD_CONFIG = -T$(prefix)/share/vessel/tls.ld

RUNTIME_RESHIM_LIBS = -lpthread  -lreshim -lveruntime -lveinterface
RUNTIME_RESHIM_LIBS += -L$(prefix)/lib64 -ldl

ifeq ($(CONFIG_DIRECTPATH),y)
MLX5_LIBS = -L$(prefix)/lib64/vessel/rdma-core/lib/statics/
MLX5_LIBS += -L$(prefix)/lib64/vessel/rdma-core/util/
MLX5_LIBS += -L$(prefix)/lib64/vessel/rdma-core/ccan/
MLX5_LIBS += -lmlx5 -libverbs -lnl-3 -lnl-route-3 -lrdmacm -lrdma_util -lccan
RUNTIME_LIBS += $(MLX5_LIBS)
RUNTIME_RESHIM_LIBS += $(MLX5_LIBS)
endif

RUNTIME_CONFIG = -D$(CONFIG_NR_CPUS) -D$(CONFIG_NODES_SHIFT)

RUNTIME_PREFIX = $(prefix)

print-%  : ; @echo $* = $($*) 

MLX5_INC = -I$(ROOT_PATH)/third_party/rdma-core/build/include
MLX5_LIBS = -L$(ROOT_PATH)/third_party/rdma-core/build/lib/
MLX5_LIBS += -L$(ROOT_PATH)/third_party/rdma-core/build/lib/statics/
MLX5_LIBS += -L$(ROOT_PATH)/third_party/rdma-core/build/util/
MLX5_LIBS += -L$(ROOT_PATH)/third_party/rdma-core/build/ccan/
MLX5_LIBS += -l:libmlx5.a -l:libibverbs.a -lnl-3 -lnl-route-3 -lrdmacm -lrdma_util -lccan

DPDK_CONFIG_PATH:=$(shell find  $(ROOT_PATH)/third_party/dpdk/build -name "pkgconfig" | head -n1)
RDMA_CORE_CONFIG_PATH:=$(shell find  $(ROOT_PATH)/third_party/rdma-core/build -name "pkgconfig" | head -n1)
PKG_CONFIG_PATH:=$(strip $(DPDK_CONFIG_PATH)):$(strip $(RDMA_CORE_CONFIG_PATH))


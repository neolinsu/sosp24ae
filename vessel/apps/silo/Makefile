ROOT_PATH = $(SHENANGODIR)
-include $(SHENANGODIR)/shared.mk

#IX_DIR ?= ../zygos
#MUTILATE_DIR ?= ../mutilate
SILO_DIR = silo
SILO_BUILD_DIR ?= $(SILO_DIR)/out-perf.masstree

CPPFLAGS = -Wall -O3 -g -MD

SILO_OBJS_ = allocator.o btree.o core.o counter.o memory.o rcu.o stats_server.o thread.o ticker.o tuple.o txn_btree.o txn.o txn_proto2_impl.o varint.o compiler.o str.o string.o straccum.o json.o benchmarks/bdb_wrapper.o benchmarks/bench.o benchmarks/encstress.o benchmarks/bid.o benchmarks/masstree/kvrandom.o benchmarks/queue.o benchmarks/tpcc.o benchmarks/ycsb.o
SILO_OBJS = $(SILO_OBJS_:%.o=$(SILO_BUILD_DIR)/%.o)

all: silotpcc-linux

silotpcc-linux: silotpcc-linux.o common-linux.o silotpcc.o $(SILO_OBJS)
	$(CXX) -o $@ $^ -L$(SILO_DIR)/third-party/lz4 -llz4 -lnuma -ljemalloc -ldb_cxx -pthread

silotpcc-ix: silotpcc-ix.o common-ix.o silotpcc.o $(SILO_OBJS) $(IX_DIR)/libix/libix.a
	$(CXX) -o $@ $^ -L$(SILO_DIR)/third-party/lz4 -llz4 -lnuma -ljemalloc -ldb_cxx -pthread

spin-linux: spin-linux.o common-linux.o generator.o $(MUTILATE_DIR)/Generator.o
	$(CXX) -o $@ $^ -pthread -lm

spin-ix: spin-ix.o common-ix.o generator.o $(MUTILATE_DIR)/Generator.o $(IX_DIR)/libix/libix.a
	$(CXX) -o $@ $^ -pthread -lm

generator.o: CPPFLAGS += -include $(MUTILATE_DIR)/Generator.h
silotpcc.o: CPPFLAGS += -I$(SILO_DIR) -I$(SILO_DIR)/third-party/lz4
spin-ix.o: CPPFLAGS += -I$(IX_DIR)/inc -I$(IX_DIR)/libix
silotpcc-ix.o: CPPFLAGS += -I$(IX_DIR)/inc -I$(IX_DIR)/libix
common-ix.o: CPPFLAGS += -I$(IX_DIR)/inc -I$(IX_DIR)/libix
common-shenango.o: CPPFLAGS += $(INC) $(CXXFLAGS) -I$(ROOT_PATH)/bindings/cc

silotpcc-shenango: silotpcc-shenango.o common-shenango.o silotpcc.o $(SILO_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) ${SHENANGODIR}/shim/libshim.a -ldl $(RUNTIME_LIBS) $(ROOT_PATH)/bindings/cc/librt++.a -L$(SILO_DIR)/third-party/lz4 -llz4 -lnuma -ljemalloc -ldb_cxx -pthread


clean:
	rm -f *.o *.d silotpcc-linux silotpcc-ix spin-linux spin-ix silotpcc-shenango

-include *.d

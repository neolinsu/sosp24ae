# Makefile for C++ bindings
include $(ROOT_PATH)/scripts/share.mk
CXXFLAGS = -Wall -O3 -fPIE -MD -D$(CONFIG_NR_CPUS) -D$(CONFIG_NODES_SHIFT) -T $(ROOT_PATH)/src/runtime/tls/tls.ld -I$(ROOT_PATH)/include/vessel/ -I$(ROOT_PATH)/src/inc -I$(ROOT_PATH)/src/inc/runtime
LDXX	= g++
LDFLAGS = -T $(ROOT_PATH)/src/runtime/tls/tls.ld  
RUNTIME_LIBS = -L$(ROOT_PATH)/src/simple_shim  -L$(ROOT_PATH)/src/runtime -L$(ROOT_PATH)/src/interface -lshim -lveruntime -lveinterface -lpthread -ldl 
rt_src = runtime.cc thread.cc
rt_obj = $(rt_src:.cc=.o)

test_src = test.cc
test_obj = $(test_src:.cc=.o)


ifeq ($(CONFIG_MLX5),y)
CXXFLAGS += -DMLX5 $(MLX5_INC)
RUNTIME_LIBS += $(MLX5_LIBS)
endif

# must be first
all: librt++.a test


librt++.a: $(rt_obj)
	$(AR) rcs $@ $^

test: $(test_obj) librt++.a $(RUNTIME_DEPS)
	$(LDXX) $(LDFLAGS) -o $@ $(test_obj) librt++.a $(RUNTIME_LIBS)

# general build rules for all targets
src = $(rt_src) $(test_src)
obj = $(src:.cc=.o)
dep = $(obj:.o=.d)

ifneq ($(MAKECMDGOALS),clean)
-include $(dep)   # include all dep files in the makefile
endif

# rule to generate a dep file by using the C preprocessor
# (see man cpp for details on the -MM and -MT options)
%.d: %.cc
	@$(CXX) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(obj) $(dep) librt++.a test

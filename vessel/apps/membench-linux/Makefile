all_src=$(wildcard ./src/*.c)
main_src=./src/main.c
cal_src=./src/cal.c
cal_linux_src=./src/cal-linux.c
gather_src=./src/gather.c
calgather_src=./src/calgather.c
probe_src=$(wildcard ./test/probe/*.c)
test_src=$(wildcard ./test/*.c)
test_bin=$(test_src:.c=)
test_script=$(foreach bin,$(test_bin),$(bin) &&)
test_script+= echo TESTEND
CFLAGS = -Wall -Werror -fPIE -ftls-model=local-exec -Wall -Werror -std=gnu11 -O3 -I./src/inc -I./src
LDFLAGS = -lm -pie

VESSEL_LIBS_CONFIG=$(shell make -C../../ print-RUNTIME_LIBS | grep RUNTIME_LIBS)
VESSEL_INC_CONFIG=$(shell make -C../../ print-RUNTIME_INC | grep RUNTIME_INC)
VESSEL_RUNTIME_CONFIG=$(shell make -C../../ print-RUNTIME_CONFIG | grep RUNTIME_CONFIG)
VESSEL_LD_CONFIG=$(shell make -C../../ print-RUNTIME_LD_CONFIG | grep RUNTIME_LD_CONFIG)
VESSEL_LIBS=$(filter-out RUNTIME_LIBS =, $(VESSEL_LIBS_CONFIG))
VESSEL_INC=$(filter-out RUNTIME_INC =, $(VESSEL_INC_CONFIG))
VESSEL_RUNTIME=$(filter-out RUNTIME_CONFIG =, $(VESSEL_RUNTIME_CONFIG))
VESSEL_LD=$(filter-out RUNTIME_LD_CONFIG =, $(VESSEL_LD_CONFIG))

%.d: %.c
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

%.d: %.S
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

%.o: %.S
	$(CC)  -c $< -o $@ $(CFLAGS)


ifeq ($(MAKECMDGOALS),membench)
CFLAGS+=$(VESSEL_INC)
endif

membench: $(main_src) 
	gcc $(CFLAGS) $(main_src) -o $@ $(LDFLAGS)

calbench: $(cal_src) 
	gcc $(CFLAGS) $(cal_src) $(runtime_based_src) -o $@ -lm $(LDFLAGS)

callinuxbench: $(cal_linux_src) 
	gcc $(CFLAGS) $(cal_linux_src) -o $@ -lm $(LDFLAGS)


memgather: $(gather_src)
	gcc -g $(CFLAGS) $(gather_src) -o $@ $(LDFLAGS) -lpthread

calgather: $(calgather_src)
	gcc -g $(CFLAGS) $(calgather_src) -o $@ $(LDFLAGS) -lpthread


pc_probe: $(probe_src)
	gcc -g $(CFLAGS) $(probe_src) -o $@ $(LDFLAGS)

clean:
	rm -f $(test_bin) membench pc_probe calgather calbench callinuxbench memgather

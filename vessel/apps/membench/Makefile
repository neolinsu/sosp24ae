all_src=$(wildcard ./src/*.c)
main_src=./src/main.c
runtime_based_src=$(main_src)
probe_src=$(wildcard ./test/probe/*.c)
src=$(filter-out $(runtime_based_src),$(all_src))
test_src=$(wildcard ./test/*.c)
test_bin=$(test_src:.c=)
test_script=$(foreach bin,$(test_bin),$(bin) &&)
test_script+= echo TESTEND
CFLAGS = -Wall -Werror -fPIE -ftls-model=local-exec -Wall -Werror -std=gnu11 -O3 -I./src/inc -mfsgsbase
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

ifneq ($(MAKECMDGOALS),pc_probe)
CFLAGS+=$(VESSEL_INC) $(VESSEL_RUNTIME)
LDFLAGS+=$(VESSEL_LIBS) $(VESSEL_LD)
endif

membench: $(runtime_based_src) $(src)
	gcc $(CFLAGS) $(src) $(runtime_based_src) -o $@ $(LDFLAGS)

pc_probe: $(probe_src)
	gcc -g $(CFLAGS) $(probe_src) -o $@ $(LDFLAGS)

clean:
	rm -f $(test_bin) membench pc_probe

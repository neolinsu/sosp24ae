all_src=$(wildcard ./src/*.c)
cache_src=./src/cache.c
cache2_src=./src/cache2.c
CFLAGS = $(MYCFLAGS) -Wall -Werror -fPIE -ftls-model=local-exec -Wall -Werror -std=gnu11 -O3 -ffast-math -march=native -I./src/inc -I./src
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

CFLAGS+=$(VESSEL_INC) $(VESSEL_RUNTIME)
LDFLAGS+=$(VESSEL_LIBS) $(VESSEL_LD)

cache: $(cache_src) 
	gcc $(CFLAGS) $(cache_src) $(runtime_based_src) -o $@ $(LDFLAGS)

cache2: $(cache2_src) 
	gcc $(CFLAGS) $(cache2_src) $(runtime_based_src) -o $@ $(LDFLAGS)


clean:
	rm -f cache cache2

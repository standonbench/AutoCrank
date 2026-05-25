# Host-side build of AutoCrank's selftest suite. Links the rebalanced game
# logic against minimal stubs (see stubs.c) so the suite runs as a plain
# Mac binary — no Playdate simulator required.

SDK := /Users/Pierre/Developer/PlaydateSDK
SRC := ../../src

CC := clang
CFLAGS := -g -O0 -Wall -Wno-unused -Wno-pointer-sign \
          -I$(SRC) -I$(SDK)/C_API \
          -DTARGET_SIMULATOR=1 -DTARGET_EXTENSION=1 -DDEBUG_BUILD=1

SOURCES := main.c stubs.c \
           $(SRC)/pets.c $(SRC)/battle.c $(SRC)/shop.c \
           $(SRC)/triggers.c $(SRC)/ai.c $(SRC)/selftest.c

host_selftest: $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@

# Stress / playtest binary — adds stress.c, drops selftest.c (own main).
STRESS_SOURCES := stress.c stubs.c \
                  $(SRC)/pets.c $(SRC)/battle.c $(SRC)/shop.c \
                  $(SRC)/triggers.c $(SRC)/ai.c

host_stress: $(STRESS_SOURCES)
	$(CC) $(CFLAGS) $(STRESS_SOURCES) -o $@

run: host_selftest
	./host_selftest

stress: host_stress
	./host_stress

clean:
	rm -f host_selftest host_stress

.PHONY: run stress clean

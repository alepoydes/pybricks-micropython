# SPDX-License-Identifier: MIT
BUILD ?= build
PBIO = ../../lib/pbio
CPPFLAGS += -I. -I$(PBIO)/include -I../../lib/lego
CFLAGS += -std=gnu99 -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter
LDLIBS += -lm
SRC = main.c $(PBIO)/src/imu.c $(PBIO)/src/geometry.c
HEADERS = $(wildcard *.h $(PBIO)/include/pbio/*.h $(PBIO)/include/pbdrv/*.h $(PBIO)/include/pbsys/*.h)

.PHONY: check
check: $(BUILD)/regression
	$(BUILD)/regression

$(BUILD):
	mkdir -p $@

$(BUILD)/regression: $(SRC) $(HEADERS) Makefile | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@

# xyz - small computer algebra system on top of bcl (bc's math library)
# Copyright (C) 2026 Gunnar Lindholm
# SPDX-License-Identifier: GPL-3.0-or-later

# Location of the bc source tree (https://github.com/gavinhoward/bc).
BC_DIR ?= ../bc
BUILD  ?= build

CXX      ?= c++
CXXFLAGS ?= -O2 -g
# _XOPEN_SOURCE makes LONG_BIT visible, so bcl.h agrees with the library
# on the size of BclBigDig.
WARN     := -std=c++17 -D_XOPEN_SOURCE=700 -Wall -Wextra -Wpedantic
SAN      := -fsanitize=address,undefined -fno-omit-frame-pointer

BC_ABS  := $(abspath $(BC_DIR))
BCL_DIR := $(BUILD)/bcl
BCL_LIB := $(BCL_DIR)/bin/libbcl.a

# Line editing (arrow keys, history) when typing at a terminal, with GNU
# readline (or libedit's compatible version) if it is available.
# make READLINE=no  builds without it.
READLINE ?= $(shell printf '\043include <cstdio>\n\043include <readline/readline.h>\nint main(){return readline("")!=0;}\n' \
              | $(CXX) -x c++ - -lreadline -o /dev/null 2>/dev/null && echo yes || echo no)
ifeq ($(READLINE),yes)
  RL_FLAGS := -DXYZ_READLINE
  RL_LIBS  := -lreadline
endif

SRCS := $(wildcard src/*.cpp)
HDRS := $(wildcard src/*.hpp)
OBJS := $(SRCS:src/%.cpp=$(BUILD)/obj/%.o)
ASAN_OBJS := $(SRCS:src/%.cpp=$(BUILD)/asan/%.o)

.PHONY: all test test-asan clean distclean

all: xyz

# Build bcl out-of-tree so the bc checkout is never modified.
$(BCL_LIB):
	@test -f $(BC_ABS)/configure.sh || \
	  { echo "bc sources not found in $(BC_ABS); use make BC_DIR=..."; exit 1; }
	mkdir -p $(BCL_DIR)
	cd $(BCL_DIR) && env -u CFLAGS $(BC_ABS)/configure.sh -a -O2 >/dev/null
	$(MAKE) -C $(BCL_DIR)

$(BUILD)/obj/%.o: src/%.cpp $(HDRS) $(BCL_LIB) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(WARN) $(CXXFLAGS) $(RL_FLAGS) -I$(BC_ABS)/include -c -o $@ $<

$(BUILD)/asan/%.o: src/%.cpp $(HDRS) $(BCL_LIB) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(WARN) -O1 -g $(SAN) $(RL_FLAGS) -I$(BC_ABS)/include -c -o $@ $<

# rebuild main.o when READLINE changes
RL_STAMP := $(BUILD)/readline-$(READLINE)
$(RL_STAMP):
	@mkdir -p $(BUILD) && rm -f $(BUILD)/readline-* && touch $@
$(BUILD)/obj/main.o $(BUILD)/asan/main.o: $(RL_STAMP)

xyz: $(OBJS) $(BCL_LIB)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(BCL_LIB) $(RL_LIBS)

$(BUILD)/xyz-asan: $(ASAN_OBJS) $(BCL_LIB)
	$(CXX) $(SAN) -o $@ $(ASAN_OBJS) $(BCL_LIB) $(RL_LIBS)

test: xyz
	python3 tests/run_tests.py ./xyz
	python3 tests/cli_tests.py ./xyz
	python3 tests/random_tests.py ./xyz

test-asan: $(BUILD)/xyz-asan
	python3 tests/run_tests.py $(BUILD)/xyz-asan
	python3 tests/cli_tests.py $(BUILD)/xyz-asan
	python3 tests/random_tests.py $(BUILD)/xyz-asan --count 60

clean:
	rm -rf xyz $(BUILD)/obj $(BUILD)/asan $(BUILD)/xyz-asan

distclean: clean
	rm -rf $(BUILD)

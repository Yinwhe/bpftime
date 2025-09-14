# SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
# Makefile for bpftime-libhelper example
# This example demonstrates using bpftime as a library runtime

OUTPUT := .output
CLANG ?= clang
CXX ?= g++

# bpftime build paths
BPFTIME_ROOT := $(abspath ../..)
BPFTIME_BUILD := $(BPFTIME_ROOT)/build
BPFTIME_LIB := $(BPFTIME_BUILD)/libbpftime.a

# libbpf paths (using bpftime's built libbpf)
LIBBPF_SRC := $(BPFTIME_ROOT)/third_party/libbpf/src
LIBBPF_OBJ := $(BPFTIME_BUILD)/libbpf/libbpf.a

# bpftool paths
BPFTOOL_SRC := $(BPFTIME_ROOT)/third_party/bpftool/src
BPFTOOL_OUTPUT ?= $(BPFTIME_BUILD)/bpftool
BPFTOOL ?= $(BPFTOOL_OUTPUT)/bpftool

# Frida paths (using bpftime's built Frida)
FRIDA_CORE_DIR := $(BPFTIME_BUILD)/FridaCore-prefix/src/FridaCore
FRIDA_GUM_DIR := $(BPFTIME_BUILD)/FridaGum-prefix/src/FridaGum
FRIDA_CORE_LIB := $(FRIDA_CORE_DIR)/libfrida-core.a
FRIDA_GUM_LIB := $(FRIDA_GUM_DIR)/libfrida-gum.a

# Architecture detection
ARCH ?= $(shell uname -m | sed 's/x86_64/x86/' \
			 | sed 's/arm.*/arm/' \
			 | sed 's/aarch64/arm64/' \
			 | sed 's/ppc64le/powerpc/' \
			 | sed 's/mips.*/mips/' \
			 | sed 's/riscv64/riscv/' \
			 | sed 's/loongarch64/loongarch/')

# vmlinux header
VMLINUX := $(BPFTIME_ROOT)/third_party/vmlinux/$(ARCH)/vmlinux.h

# Include paths for eBPF compilation
BPF_INCLUDES := -I$(OUTPUT) \
		-I$(BPFTIME_ROOT)/third_party/libbpf/include/uapi \
		-I$(dir $(VMLINUX))

# Include paths for C++ compilation
CXX_INCLUDES := -I$(OUTPUT) \
		-Iinclude \
		-I$(BOOST_INCLUDEDIR) \
		-I$(BPFTIME_ROOT)/runtime/include \
		-I$(BPFTIME_ROOT)/runtime/src \
		-I$(BPFTIME_ROOT)/runtime/syscall-server \
		-I$(BPFTIME_ROOT)/vm/vm-core/include \
		-I$(BPFTIME_ROOT)/vm/compat/include \
		-I$(BPFTIME_ROOT)/attach/frida_uprobe_attach_impl/include \
		-I$(BPFTIME_ROOT)/third_party/spdlog/include \
		-I$(BPFTIME_ROOT)/third_party/libbpf/include \
		-I$(BPFTIME_ROOT)/third_party/libbpf/include/uapi \
		-I$(FRIDA_CORE_DIR) \
		-I$(FRIDA_GUM_DIR)

# Compiler flags
CXXFLAGS := -g -Wall -std=c++20 -O2 \
	    -Wno-unused-parameter \
	    -Wno-unused-variable
CFLAGS := -g -Wall
BPF_CFLAGS := -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH)

# Linker flags
LDFLAGS := -lrt -ldl -lpthread -lelf -lz

# Get Clang's default includes for BPF compilation
CLANG_BPF_SYS_INCLUDES ?= $(shell $(CLANG) -v -E - </dev/null 2>&1 \
	| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')

# Applications to build
APPS = function_call_monitor
TARGETS = $(APPS) test_target

# Source files (updated paths)
MONITOR_SOURCES = src/main.cpp src/function_monitor.cpp
MONITOR_OBJECTS = $(MONITOR_SOURCES:%.cpp=$(OUTPUT)/%.o)

# Verbose output control
ifeq ($(V),1)
	Q =
	msg =
else
	Q = @
	msg = @printf '  %-8s %s%s\n'					\
		      "$(1)"						\
		      "$(patsubst $(abspath $(OUTPUT))/%,%,$(2))"	\
		      "$(if $(3), $(3))";
	MAKEFLAGS += --no-print-directory
endif

.PHONY: all clean

all: $(TARGETS)

# Create output directories
$(OUTPUT) $(OUTPUT)/src:
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

# Build test_target (simple C program)
test_target: test/test_target.c
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $< -o $@

# Build BPF object file
$(OUTPUT)/function_counter.bpf.o: bpf/function_counter.bpf.c $(VMLINUX) | $(OUTPUT)
	$(call msg,BPF,$@)
	$(Q)$(CLANG) $(BPF_CFLAGS) $(BPF_INCLUDES) $(CLANG_BPF_SYS_INCLUDES) \
		     -c $< -o $@

# Generate BPF skeleton header
$(OUTPUT)/function_counter.skel.h: $(OUTPUT)/function_counter.bpf.o
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

# Build C++ object files
$(OUTPUT)/%.o: %.cpp $(OUTPUT)/function_counter.skel.h | $(OUTPUT) $(OUTPUT)/src
	$(call msg,CXX,$@)
	$(Q)$(CXX) $(CXXFLAGS) $(CXX_INCLUDES) -c $< -o $@

# Build the main application
function_call_monitor: $(MONITOR_OBJECTS) $(BPFTIME_LIB) $(LIBBPF_OBJ) | $(OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CXX) $(CXXFLAGS) $(MONITOR_OBJECTS) \
		$(BPFTIME_LIB) $(LIBBPF_OBJ) \
		$(FRIDA_CORE_LIB) $(FRIDA_GUM_LIB) \
		$(LDFLAGS) -o $@

# Clean build artifacts
clean:
	$(call msg,CLEAN)
	$(Q)rm -rf $(OUTPUT) $(TARGETS)
	$(Q)rm -f *.log *.tmp

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@test -f $(BPFTIME_LIB) || (echo "ERROR: bpftime library not found at $(BPFTIME_LIB). Please build bpftime first with: make release" && false)
	@test -f $(LIBBPF_OBJ) || (echo "ERROR: libbpf not found at $(LIBBPF_OBJ). Please build bpftime first." && false)
	@test -f $(FRIDA_CORE_LIB) || (echo "ERROR: Frida Core not found at $(FRIDA_CORE_LIB). Please build bpftime first." && false)
	@test -f $(BPFTOOL) || (echo "ERROR: bpftool not found at $(BPFTOOL). Please build bpftime first." && false)
	@echo "All dependencies found."

# Print build configuration
info:
	@echo "=== bpftime-libhelper Build Configuration ==="
	@echo "BPFTIME_ROOT: $(BPFTIME_ROOT)"
	@echo "BPFTIME_BUILD: $(BPFTIME_BUILD)"
	@echo "BPFTIME_LIB: $(BPFTIME_LIB)"
	@echo "LIBBPF_OBJ: $(LIBBPF_OBJ)"
	@echo "FRIDA_CORE_LIB: $(FRIDA_CORE_LIB)"
	@echo "BPFTOOL: $(BPFTOOL)"
	@echo "VMLINUX: $(VMLINUX)"
	@echo "ARCH: $(ARCH)"
	@echo "=========================================="

# Add dependency on skeleton for all object files
$(MONITOR_OBJECTS): $(OUTPUT)/function_counter.skel.h

# Dependencies check target
$(APPS): check-deps

.DELETE_ON_ERROR:
.SECONDARY:
# bpftime-libhelper: Using bpftime as a Library

This example demonstrates how to embed bpftime as a library runtime instead of using CLI tools. This enables direct integration of eBPF capabilities into applications.

## Core Concept

### Traditional CLI Usage
```bash
bpftime load ./my-program
bpftime start ./target-app
bpftime attach 1234
```

### Library Integration
```cpp
#include <bpftime_shm.hpp>
#include "function_counter.skel.h"

FunctionCallMonitor monitor;
monitor.monitor_function(binary_path, func_name, pid);
```

## Implementation

### bpftime CLI Core Commands

**1. `bpftime load <program>`**
- Starts syscall-server process in background
- Intercepts eBPF syscalls via LD_PRELOAD
- Creates shared memory for storing eBPF programs and maps
- Purpose: Set up userspace eBPF runtime environment

**2. `bpftime start <target>`**
- Launches target process with agent preloaded
- Agent connects to existing syscall-server
- Enables eBPF program execution in target process
- Purpose: Run target with eBPF instrumentation from startup

**3. `bpftime attach <pid>`**
- Uses Frida to inject agent into running process
- Establishes connection between agent and syscall-server
- Patches target functions for uprobe/uretprobe
- Purpose: Add eBPF instrumentation to existing process

### Library Implementation

Library mode replaces CLI workflow using two key mechanisms:

**1. Syscall Interception (replaces `bpftime load`)**

By linking `libbpftime.a`, we integrate runtime/syscall-server code that hijacks BPF syscalls:
```cpp
bool monitor_function(...) {
    // Standard libbpf calls - but syscalls are intercepted by our linked code
    skel_ = function_counter_bpf__open();    // Intercepted: creates userspace program
    function_counter_bpf__load(skel_);       // Intercepted: loads into userspace runtime
    monitor_map_ = skel_->maps.function_count_map;  // Direct access to userspace map
}
```

When libbpf calls `syscall(__NR_bpf, ...)`, our redefined `syscall()` function intercepts it and redirects to `context->handle_sysbpf()` for userspace execution.

**2. Frida Agent Injection (replaces `bpftime start/attach`)**

We programmatically inject bpftime-agent.so into target processes:
```cpp
// Replace CLI: bpftime attach <pid>
int inject_agent_to_process(int pid, const std::string& agent_path) {
    frida_init();
    FridaInjector* injector = frida_injector_new();
    frida_injector_inject_library_file_sync(
        injector, pid, agent_path.c_str(), "bpftime_agent_main", ...);
}

// After injection, uprobe attachment works transparently
uprobe_link_ = bpf_program__attach_uprobe_opts(
    skel_->progs.count_function_calls, pid, binary_path.c_str(), ...);
```

This eliminates the need for external syscall-server processes while maintaining full compatibility with standard libbpf APIs.


## Build and Run

### Prerequisites
- bpftime built with `make release`
- Boost libraries (set via environment variables)

### Build
```bash
cd example/bpftime-libhelper
make all
```

### Usage
```bash
./test_target &
./function_call_monitor $! /lib/x86_64-linux-gnu/libc.so.6 malloc
```

This example proves that bpftime can be seamlessly integrated as a library, enabling eBPF capabilities in any application without external process management or root privileges.
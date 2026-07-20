# Direct runtime/handler library example

This example embeds bpftime's current runtime and handler libraries in one
process. It does not intercept `sys_bpf`, call libbpf, or start a bpftime CLI
subprocess.

Here, library mode means that the application links bpftime's runtime/handler
archives and owns the lifecycle through their C++ APIs. It does not mean
linking the aggregate `libbpftime.a` while retaining libbpf's syscall-based
load and attach path.

The older `libbpftime_example` only imports and exports shared-memory JSON. The
previous `bpftime-libhelper` proposal linked `libbpftime.a`, libbpf, and Frida,
then used a generated libbpf skeleton whose BPF syscalls were intercepted and
injected the agent into a target process. In contrast, this example calls
bpftime's runtime/handler APIs directly.

## Lifecycle

`main.cpp` performs the complete lifecycle in a small, deterministic program:

1. Initialize bpftime shared memory and create an array map.
2. Create a program handler from 15 in-memory eBPF instructions.
3. Create a custom perf-event handler and a link from it to the program.
4. Register `simple_attach_impl`; `bpf_attach_ctx` loads the program from the
   handlers and instantiates the link.
5. Trigger the link in-process. The program updates map element zero from 41
   to 42 through the `bpf_map_update_elem` helper and returns 42.
6. Read both the program return value and the map value.
7. Detach the instantiated link, unload the runtime program, close the link,
   perf-event, program, and map handlers, then destroy and remove shared memory.

The custom in-process attach keeps the example CI-sized and avoids adding
target-binary and symbol-resolution requirements. It also avoids the additional
Linux/Frida coverage required by a real uprobe attach, without changing the
runtime/handler lifecycle being shown.

## Build and run

From the repository root, build only this example:

```sh
cmake -S . -B build \
  -DBPFTIME_ENABLE_CUDA_ATTACH=OFF \
  -DBUILD_BPFTIME_DAEMON=OFF
cmake --build build --target bpftime_runtime_library_example -j
./build/example/runtime-library/bpftime_runtime_library_example
```

The example does not call libbpf APIs, but the repository's default
`BPFTIME_BUILD_WITH_LIBBPF=ON` configuration makes `runtime` transitively link
libbpf, libelf, and zlib. That is a build configuration dependency, not the
libbpf syscall-interception execution path. A core runtime configured with
`BPFTIME_BUILD_WITH_LIBBPF=OFF` omits those link dependencies where that
configuration is supported.

At least one VM backend must be enabled. CMake selects LLVM when
`BPFTIME_LLVM_JIT=ON`, otherwise uBPF when `BPFTIME_UBPF_JIT=ON`.

A successful run prints the allocated handler IDs, `program_result=42`,
`map_value=42`, and a final cleanup message.

## Link dependencies

The CMake target links these bpftime targets:

- `runtime`: shared-memory handlers, maps, program loading/execution, helper
  groups, and `bpf_attach_ctx`;
- `bpftime_simple_attach_impl`: the small in-process attach/trigger adapter;
- the selected VM backend object, so constructor-based VM registration is not
  discarded by static linking.

Their CMake interfaces transitively provide `bpftime_vm`, the selected LLVM or
uBPF backend, spdlog, Frida Gum, Boost headers, threads, and platform libraries.
With the default `BUILD_SHARED_LIBS=OFF`, the example uses the in-tree static
archives and does not require bpftime runtime `.so` files at run time.

With `BPFTIME_BUILD_WITH_LIBBPF=ON`, the `runtime` interface also provides
libbpf, libelf, and zlib. With it disabled, zlib remains a runtime dependency,
while libbpf and libelf are omitted.

The aggregate installed `libbpftime.a` is not the target used here and is not,
by itself, an equivalent link line: `simple_attach_impl` is a separate target,
and a statically linked VM backend's registration object must be retained. Use
the repository CMake target as the reference link configuration.

The example is intentionally skipped when `BUILD_SHARED_LIBS=ON`. The current
shared `runtime` target does not export the runtime/handler API symbols needed
by an embedding application. Supporting an out-of-tree shared-library SDK
requires a separate public-symbol and CMake package/export design; it is not a
different link line for this example.

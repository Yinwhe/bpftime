if(NOT BPFTIME_LLVM_JIT AND NOT BPFTIME_UBPF_JIT)
  message(STATUS
    "Skipping bpftime_runtime_library_example: no VM backend is enabled"
  )
  return()
endif()

if(BUILD_SHARED_LIBS)
  message(STATUS
    "Skipping bpftime_runtime_library_example: the runtime/handler APIs are not exported by the current shared-library build"
  )
  return()
endif()

add_executable(bpftime_runtime_library_example main.cpp)

target_link_libraries(bpftime_runtime_library_example
  PRIVATE
  runtime
  bpftime_simple_attach_impl
)

target_include_directories(bpftime_runtime_library_example
  PRIVATE ${SIMPLE_ATTACH_IMPL_INCLUDE}
)

# VM backends register themselves from constructor-only translation units.
# Embed the selected backend object so static linking cannot discard that
# registration code.
if(BPFTIME_LLVM_JIT)
  target_sources(bpftime_runtime_library_example
    PRIVATE $<TARGET_OBJECTS:bpftime_llvm_vm_obj>
  )
  target_compile_definitions(bpftime_runtime_library_example
    PRIVATE BPFTIME_RUNTIME_LIBRARY_VM_NAME="llvm"
  )
elseif(BPFTIME_UBPF_JIT)
  target_sources(bpftime_runtime_library_example
    PRIVATE $<TARGET_OBJECTS:bpftime_ubpf_vm_obj>
  )
  target_compile_definitions(bpftime_runtime_library_example
    PRIVATE BPFTIME_RUNTIME_LIBRARY_VM_NAME="ubpf"
  )
endif()

set_property(TARGET bpftime_runtime_library_example PROPERTY CXX_STANDARD 20)

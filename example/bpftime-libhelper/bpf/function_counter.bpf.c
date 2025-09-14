#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

// Map to store function call counters
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);
    __type(value, __u64);
} function_count_map SEC(".maps");

// Helper function to update counter
static __always_inline void update_counter(__u32 key)
{
    __u64 *count = bpf_map_lookup_elem(&function_count_map, &key);
    if (count) {
        __sync_fetch_and_add(count, 1);
    } else {
        __u64 initial = 1;
        bpf_map_update_elem(&function_count_map, &key, &initial, BPF_ANY);
    }
}

// Uprobe program: count function entries
SEC("uprobe/count_function_calls")
int count_function_calls(struct pt_regs *ctx)
{
    update_counter(1); // Use key 1 for function calls
    return 0;
}

// Uretprobe program: count function returns
SEC("uretprobe/count_function_returns")
int count_function_returns(struct pt_regs *ctx)
{
    update_counter(2); // Use key 2 for function returns
    return 0;
}

char LICENSE[] SEC("license") = "GPL";
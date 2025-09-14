#ifndef FUNCTION_MONITOR_HPP
#define FUNCTION_MONITOR_HPP

#include "common.hpp"

// Forward declarations
struct bpf_link;
struct bpf_map;
struct function_counter_bpf;

class FunctionCallMonitor {
public:
    FunctionCallMonitor();
    ~FunctionCallMonitor();

    // Non-copyable
    FunctionCallMonitor(const FunctionCallMonitor&) = delete;
    FunctionCallMonitor& operator=(const FunctionCallMonitor&) = delete;

    // Initialize the monitor
    bool initialize();

    // Monitor specified function (both entry and exit)
    bool monitor_function(const std::string& binary_path, const std::string& function_name, int pid);

    // Get function call count
    uint64_t get_function_call_count();

    // Get function return count
    uint64_t get_function_return_count();

    // Detach all probes
    bool detach_all();

private:
    bool initialized_;
    struct bpf_map* monitor_map_;
    struct function_counter_bpf* skel_;
    struct bpf_link* uprobe_link_;
    struct bpf_link* uretprobe_link_;

    // Helper methods
    void cleanup();
    uint64_t get_counter_value(uint32_t key);
    std::string get_default_agent_path();
    int inject_agent_to_process(int pid, const std::string& agent_path);
};

#endif // FUNCTION_MONITOR_HPP
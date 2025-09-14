#include "function_monitor.hpp"
#include "function_counter.skel.h"

FunctionCallMonitor::FunctionCallMonitor()
    : initialized_(false), monitor_map_(nullptr), skel_(nullptr),
      uprobe_link_(nullptr), uretprobe_link_(nullptr) {
}

FunctionCallMonitor::~FunctionCallMonitor() {
    cleanup();
}

bool FunctionCallMonitor::initialize() {
    if (initialized_) {
        return true;
    }
    initialized_ = true;
    return true;
}

bool FunctionCallMonitor::monitor_function(const std::string& binary_path, const std::string& function_name, int pid) {
    if (!initialized_) {
        std::cerr << "[ERROR] Monitor not initialized" << std::endl;
        return false;
    }

    // Open and load BPF program
    skel_ = function_counter_bpf__open();
    if (!skel_) {
        std::cerr << "[ERROR] Failed to open BPF skeleton" << std::endl;
        return false;
    }

    if (function_counter_bpf__load(skel_)) {
        std::cerr << "[ERROR] Failed to load BPF program" << std::endl;
        function_counter_bpf__destroy(skel_);
        skel_ = nullptr;
        return false;
    }

    monitor_map_ = skel_->maps.function_count_map;

    // Helper function to attach probe
    auto attach_probe = [&](bool is_retprobe) -> struct bpf_link* {
        struct bpf_uprobe_opts opts = {};
        opts.sz = sizeof(opts);
        opts.retprobe = is_retprobe;
        opts.func_name = function_name.c_str();

        return bpf_program__attach_uprobe_opts(
            is_retprobe ? skel_->progs.count_function_returns : skel_->progs.count_function_calls,
            pid, binary_path.c_str(), 0, &opts);
    };

    // Attach uprobe
    uprobe_link_ = attach_probe(false);
    if (!uprobe_link_) {
        std::cerr << "[ERROR] Failed to attach uprobe to " << binary_path << ":" << function_name << std::endl;
        return false;
    }

    // Attach uretprobe
    uretprobe_link_ = attach_probe(true);
    if (!uretprobe_link_) {
        std::cerr << "[ERROR] Failed to attach uretprobe to " << binary_path << ":" << function_name << std::endl;
        bpf_link__destroy(uprobe_link_);
        uprobe_link_ = nullptr;
        return false;
    }

    // Inject agent
    std::string agent_path = get_default_agent_path();
    if (inject_agent_to_process(pid, agent_path) != 0) {
        std::cerr << "[ERROR] Agent injection failed, cannot continue" << std::endl;
        return false;
    }

    std::cout << "[INFO] Monitoring " << function_name << " on PID " << pid << std::endl;
    return true;
}

bool FunctionCallMonitor::detach_all() {
    bool success = true;

    auto detach_link = [&](struct bpf_link*& link, const char* name) {
        if (link) {
            if (bpf_link__destroy(link)) {
                std::cerr << "[ERROR] Failed to destroy " << name << " link" << std::endl;
                success = false;
            }
            link = nullptr;
        }
    };

    detach_link(uprobe_link_, "uprobe");
    detach_link(uretprobe_link_, "uretprobe");
    return success;
}

uint64_t FunctionCallMonitor::get_function_call_count() {
    return get_counter_value(1); // uprobe uses key=1
}

uint64_t FunctionCallMonitor::get_function_return_count() {
    return get_counter_value(2); // uretprobe uses key=2
}

uint64_t FunctionCallMonitor::get_counter_value(uint32_t key) {
    if (!monitor_map_) {
        return 0;
    }

    uint64_t value = 0;
    bpf_map__lookup_elem(monitor_map_, &key, sizeof(key), &value, sizeof(value), 0);
    return value;
}

void FunctionCallMonitor::cleanup() {
    detach_all();

    if (skel_) {
        function_counter_bpf__destroy(skel_);
        skel_ = nullptr;
    }

    monitor_map_ = nullptr;
    initialized_ = false;
}

std::string FunctionCallMonitor::get_default_agent_path() {
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.bpftime/libbpftime-agent.so";
    }
    return "/root/.bpftime/libbpftime-agent.so";
}

int FunctionCallMonitor::inject_agent_to_process(int pid, const std::string& agent_path) {
    if (access(agent_path.c_str(), F_OK) != 0) {
        std::cerr << "[ERROR] Agent file not found: " << agent_path << std::endl;
        return -1;
    }

    frida_init();
    FridaInjector* injector = frida_injector_new();
    if (!injector) {
        std::cerr << "[ERROR] Failed to create Frida injector" << std::endl;
        return -1;
    }

    GError* error = nullptr;
    frida_injector_inject_library_file_sync(
        injector, pid, agent_path.c_str(), "bpftime_agent_main", "", nullptr, &error);

    if (error) {
        std::cerr << "[ERROR] Agent injection failed: " << error->message << std::endl;
        g_error_free(error);
        g_object_unref(injector);
        return -1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    g_object_unref(injector);
    return 0;
}
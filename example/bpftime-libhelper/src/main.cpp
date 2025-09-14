#include "function_monitor.hpp"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>
#include <unistd.h>

static std::unique_ptr<FunctionCallMonitor> g_monitor;

void print_usage(const char* program_name) {
    std::cout << "Function Call Monitor - Monitor function entry and exit\n" << std::endl;
    std::cout << "Usage: " << program_name << " <PID> <LIBRARY_PATH> <FUNCTION_NAME>" << std::endl;
    std::cout << "\nExample:" << std::endl;
    std::cout << "  " << program_name << " 1234 /lib/x86_64-linux-gnu/libc.so.6 malloc" << std::endl;
}

void cleanup_and_exit(int exit_code = 0) {
    if (g_monitor) {
        g_monitor->detach_all();
        g_monitor.reset();
    }
    exit(exit_code);
}

void signal_handler(int sig) {
    std::cout << "\n[INFO] Received signal, stopping..." << std::endl;
    cleanup_and_exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    int pid = std::atoi(argv[1]);
    if (pid <= 0) {
        std::cerr << "[ERROR] Invalid PID: " << argv[1] << std::endl;
        return 1;
    }
    
    std::string library_path = argv[2];
    std::string function_name = argv[3];

    // 验证参数
    if (function_name.empty()) {
        std::cerr << "[ERROR] Function name cannot be empty" << std::endl;
        return 1;
    }

    // 验证库文件是否存在
    if (access(library_path.c_str(), F_OK) != 0) {
        std::cerr << "[ERROR] Library file not found: " << library_path << std::endl;
        return 1;
    }

    // 验证库文件是否可读
    if (access(library_path.c_str(), R_OK) != 0) {
        std::cerr << "[ERROR] Library file not readable: " << library_path << std::endl;
        return 1;
    }

    // 验证目标进程是否存在
    if (kill(pid, 0) != 0) {
        std::cerr << "[ERROR] Target process " << pid << " does not exist or not accessible" << std::endl;
        return 1;
    }

    // 设置日志输出
    setenv("BPFTIME_LOG_OUTPUT", "./runtime.log", 1);
    
    std::cout << "[INFO] Function Call Monitor Starting" << std::endl;
    std::cout << "[INFO] Target: PID " << pid << " - " << library_path << ":" << function_name << std::endl;

    // Create and setup monitor
    g_monitor = std::make_unique<FunctionCallMonitor>();
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::atexit([]() {
        if (g_monitor) {
            g_monitor->detach_all();
            g_monitor.reset();
        }
    });

    // Initialize and start monitoring
    if (!g_monitor->initialize() ||
        !g_monitor->monitor_function(library_path, function_name, pid)) {
        std::cerr << "[ERROR] Failed to start monitoring" << std::endl;
        return 1;
    }

    std::cout << "[INFO] Monitoring active, press Ctrl+C to stop..." << std::endl;

    // Monitor loop
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (kill(pid, 0) != 0) {
            std::cout << "[INFO] Target process exited, stopping..." << std::endl;
            break;
        }

        uint64_t calls = g_monitor->get_function_call_count();
        uint64_t returns = g_monitor->get_function_return_count();
        std::cout << function_name << ": called " << calls << " times, returned "
                  << returns << " times" << std::endl;
    }

    cleanup_and_exit(0);

    return 0;
}
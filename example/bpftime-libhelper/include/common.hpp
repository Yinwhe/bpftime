#pragma once

// 标准库头文件
#include <iostream>
#include <string>
#include <filesystem>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>
#include <vector>
#include <iomanip>
#include <fstream>
#include <memory>
#include <signal.h>

// libbpf 标准头文件
extern "C" {
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
}

// bpftime 核心头文件
#include <bpftime_shm.hpp>
#include <bpftime_shm_internal.hpp>
#include <spdlog/spdlog.h>

// Frida 头文件用于 agent 注入
#include <frida-core.h>

// syscall-server 头文件
#include "syscall_context.hpp"
#include "syscall_server_utils.hpp"
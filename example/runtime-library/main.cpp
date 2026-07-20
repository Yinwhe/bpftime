#if __APPLE__
#include "spinlock_wrapper.hpp"
#endif

#include "bpf_attach_ctx.hpp"
#include "bpftime_config.hpp"
#include "bpftime_shm.hpp"
#include "simple_attach_impl.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <ebpf-vm.h>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace
{

constexpr int kAttachType = 2001;
constexpr std::uint32_t kInitialValue = 41;
constexpr std::uint32_t kExpectedValue = 42;

std::array<ebpf_inst, 15> make_map_update_program(int map_fd)
{
	return {
		// key = 0 and value = 42 on the eBPF stack.
		ebpf_inst{ EBPF_OP_MOV64_IMM, 0, 0, 0, 0 },
		ebpf_inst{ EBPF_OP_STXW, 10, 0, -4, 0 },
		ebpf_inst{ EBPF_OP_MOV64_IMM, 0, 0, 0, kExpectedValue },
		ebpf_inst{ EBPF_OP_STXW, 10, 0, -8, 0 },
		// r1 = map_fd (BPF_PSEUDO_MAP_FD), r2 = &key,
		// r3 = &value, r4 = BPF_ANY.
		ebpf_inst{ EBPF_OP_LDDW, 1, 1, 0, map_fd },
		ebpf_inst{ 0, 0, 0, 0, 0 },
		ebpf_inst{ EBPF_OP_MOV64_REG, 2, 10, 0, 0 },
		ebpf_inst{ EBPF_OP_ADD64_IMM, 2, 0, 0, -4 },
		ebpf_inst{ EBPF_OP_MOV64_REG, 3, 10, 0, 0 },
		ebpf_inst{ EBPF_OP_ADD64_IMM, 3, 0, 0, -8 },
		ebpf_inst{ EBPF_OP_MOV64_IMM, 4, 0, 0, 0 },
		// bpf_map_update_elem(map, &key, &value, BPF_ANY).
		ebpf_inst{ EBPF_OP_CALL, 0, 0, 0, 2 },
		// Preserve a helper error; otherwise return the stored value.
		ebpf_inst{ EBPF_OP_JNE_IMM, 0, 0, 1, 0 },
		ebpf_inst{ EBPF_OP_MOV64_IMM, 0, 0, 0, kExpectedValue },
		ebpf_inst{ EBPF_OP_EXIT, 0, 0, 0, 0 },
	};
}

} // namespace

int main()
{
	using namespace bpftime;

	int map_fd = -1;
	int prog_fd = -1;
	int perf_event_fd = -1;
	int link_fd = -1;
	bool shm_initialized = false;
	bool link_instantiated = false;
	std::unique_ptr<bpf_attach_ctx> attach_ctx;

	auto cleanup = [&]() {
		int detach_error = 0;
		if (attach_ctx) {
			if (link_instantiated) {
				detach_error =
					attach_ctx
						->destroy_instantiated_attach_link(
							link_fd);
				link_instantiated = false;
			}
			// Dropping instantiated programs unloads their VM code.
			attach_ctx->reset_instantiated_state();
		}
		if (link_fd >= 0)
			bpftime_close(link_fd);
		link_fd = -1;
		if (perf_event_fd >= 0)
			bpftime_close(perf_event_fd);
		perf_event_fd = -1;
		if (prog_fd >= 0)
			bpftime_close(prog_fd);
		prog_fd = -1;
		if (map_fd >= 0)
			bpftime_close(map_fd);
		map_fd = -1;
		if (shm_initialized) {
			bpftime_destroy_global_shm();
			bpftime_remove_global_shm();
			shm_initialized = false;
		}
		return detach_error;
	};

	auto fail = [&](const std::string &message) {
		std::cerr << "error: " << message << '\n';
		cleanup();
		return 1;
	};

	try {
		bpftime_initialize_global_shm(
			shm_open_type::SHM_REMOVE_AND_CREATE);
		shm_initialized = true;

		map_fd = bpftime_maps_create(
			-1, "runtime_library_counter",
			bpf_map_attr{
				.type = static_cast<int>(
					bpf_map_type::BPF_MAP_TYPE_ARRAY),
				.key_size = sizeof(std::uint32_t),
				.value_size = sizeof(std::uint32_t),
				.max_ents = 1,
			});
		if (map_fd < 0)
			return fail("failed to create the counter map");

		const std::uint32_t key = 0;
		const std::uint32_t initial_value = kInitialValue;
		if (bpftime_map_update_elem(map_fd, &key, &initial_value, 0) <
		    0)
			return fail("failed to initialize the counter map");

		const auto instructions = make_map_update_program(map_fd);
		prog_fd = bpftime_progs_create(
			-1, instructions.data(), instructions.size(),
			"runtime_library_update",
			static_cast<int>(
				bpf_prog_type::BPF_PROG_TYPE_SOCKET_FILTER));
		if (prog_fd < 0)
			return fail("failed to create the program handler");

		perf_event_fd = bpftime_add_custom_perf_event(kAttachType,
							      "counter-map");
		if (perf_event_fd < 0)
			return fail(
				"failed to create the custom perf-event handler");

		link_fd = bpftime_attach_perf_to_bpf(perf_event_fd, prog_fd);
		if (link_fd < 0)
			return fail("failed to create the program link");

		attach_ctx = std::make_unique<bpf_attach_ctx>();
		std::uint64_t execution_result = 0;
		auto trigger =
			simple_attach::add_simple_attach_impl_to_attach_ctx(
				kAttachType,
				[&](const std::string &attach_argument,
				    const std::string &trigger_argument,
				    const attach::ebpf_run_callback
					    &run_program) {
					if (attach_argument != "counter-map" ||
					    trigger_argument != "update")
						return -EINVAL;
					std::uint32_t invocation_context = 0;
					return run_program(
						&invocation_context,
						sizeof(invocation_context),
						&execution_result);
				},
				*attach_ctx);

		auto runtime_config = bpftime_get_agent_config();
		runtime_config.set_vm_name(BPFTIME_RUNTIME_LIBRARY_VM_NAME);
		runtime_config.jit_enabled = false;
		auto shared_config = runtime_config;
		bpftime_set_agent_config(std::move(shared_config));
		if (attach_ctx->init_attach_ctx_from_handlers(runtime_config) <
		    0)
			return fail("failed to load and attach the program");
		link_instantiated = true;

		if (trigger("update") != 0)
			return fail("failed to invoke the attached program");

		const auto *map_value = static_cast<const std::uint32_t *>(
			bpftime_map_lookup_elem(map_fd, &key));
		if (!map_value)
			return fail("failed to read the counter map");
		if (execution_result != kExpectedValue ||
		    *map_value != kExpectedValue)
			return fail("unexpected program or map result");

		std::cout << "map_fd=" << map_fd << " prog_fd=" << prog_fd
			  << " perf_event_fd=" << perf_event_fd
			  << " link_fd=" << link_fd << '\n';
		std::cout << "program_result=" << execution_result
			  << " map_value=" << *map_value << '\n';

		if (attach_ctx->destroy_instantiated_attach_link(link_fd) < 0)
			return fail("failed to detach the program link");
		link_instantiated = false;
		if (cleanup() < 0)
			return fail("cleanup reported a detach failure");
		std::cout << "detached and removed all runtime resources\n";
		return 0;
	} catch (const std::exception &error) {
		return fail(error.what());
	}
}

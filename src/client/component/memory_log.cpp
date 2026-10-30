#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "d3d11/texture_diagnostics.hpp"
#include "game/game.hpp"
#include "game/dvars.hpp"
#include "command.hpp"
#include "console.hpp"
#include "scheduler.hpp"

#include <version.h>
#include <cstdio>
#include <dxgi1_4.h>
#include <wrl/client.h>
#pragma comment(lib, "dxgi.lib")

// TEMPORARY testing aid; see docs/vr-memory-log.md for how to remove it.
// Periodic memory log for diagnosing Create2DTexture E_OUTOFMEMORY failures.
// Records this process's committed memory, the system commit charge and limit
// (RAM plus page file) and the GPU's per-process local/shared usage against its
// budget, plus an immediate line for every failed texture creation. The file
// survives the game's fatal error dialog and can be shared as-is.
namespace memory_log
{
	namespace
	{
		constexpr auto path = "minidumps/overlord-memory-log.txt";
		// Kept small on purpose: past 1 MB (several hours of samples) the file
		// starts over instead of growing.
		constexpr std::uintmax_t max_bytes = 1024 * 1024;
		game::dvar_t* enabled{};
		constexpr std::chrono::seconds interval{5};
		std::mutex file_mutex;
		std::atomic_bool alive{true};
		std::atomic<std::uint64_t> texture_failures{};

		double gib(const std::uint64_t bytes)
		{
			return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
		}

		std::string timestamp()
		{
			SYSTEMTIME now{};
			GetLocalTime(&now);
			char text[32]{};
			std::snprintf(text, sizeof(text), "%02u:%02u:%02u", now.wHour, now.wMinute, now.wSecond);
			return text;
		}

		std::string map_name()
		{
			const auto* map = game::CL_IsCgameInitialized() ? game::Dvar_FindVar("mapname") : nullptr;
			return map && map->current.string && *map->current.string ? map->current.string : "menu";
		}

		std::string process_memory()
		{
			PROCESS_MEMORY_COUNTERS_EX counters{};
			counters.cb = sizeof(counters);
			if (!K32GetProcessMemoryInfo(GetCurrentProcess(),
				reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
				return "process=unavailable";
			char text[128]{};
			std::snprintf(text, sizeof(text), "game_committed=%.2fGB game_ram=%.2fGB",
				gib(counters.PrivateUsage), gib(counters.WorkingSetSize));
			return text;
		}

		std::string system_memory()
		{
			PERFORMANCE_INFORMATION info{};
			info.cb = sizeof(info);
			if (!K32GetPerformanceInfo(&info, sizeof(info))) return "system=unavailable";
			const auto page = static_cast<std::uint64_t>(info.PageSize);
			char text[160]{};
			std::snprintf(text, sizeof(text), "windows_commit=%.2f/%.2fGB ram_free=%.2f/%.2fGB",
				gib(info.CommitTotal * page), gib(info.CommitLimit * page),
				gib(info.PhysicalAvailable * page), gib(info.PhysicalTotal * page));
			return text;
		}

		// QueryVideoMemoryInfo reports this process's usage on the adapter. The
		// adapter with the most dedicated memory is the one the game renders on.
		std::string gpu_memory()
		{
			Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
			if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return "gpu=unavailable";
			Microsoft::WRL::ComPtr<IDXGIAdapter3> best;
			SIZE_T best_dedicated{};
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			for (UINT index = 0; factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index)
			{
				DXGI_ADAPTER_DESC1 description{};
				Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
				if (SUCCEEDED(adapter->GetDesc1(&description)) && !(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
					description.DedicatedVideoMemory > best_dedicated && SUCCEEDED(adapter.As(&adapter3)))
				{
					best = adapter3;
					best_dedicated = description.DedicatedVideoMemory;
				}
				adapter.Reset();
			}
			DXGI_QUERY_VIDEO_MEMORY_INFO local{}, shared{};
			if (!best || FAILED(best->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local)) ||
				FAILED(best->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &shared)))
				return "gpu=unavailable";
			char text[192]{};
			std::snprintf(text, sizeof(text),
				"gpu_dedicated=%.2fGB used=%.2fGB budget=%.2fGB gpu_shared_used=%.2f/%.2fGB",
				gib(best_dedicated), gib(local.CurrentUsage), gib(local.Budget),
				gib(shared.CurrentUsage), gib(shared.Budget));
			return text;
		}

		void append(const std::string& line)
		{
			if (enabled && !enabled->current.enabled) return;
			const std::lock_guard lock(file_mutex);
			std::error_code error;
			std::filesystem::create_directories("minidumps", error);
			const auto size = std::filesystem::file_size(path, error);
			const auto mode = !error && size > max_bytes ? std::ios::trunc : std::ios::app;
			std::ofstream file(path, std::ios::out | mode);
			if (!file) return;
			file << line << '\n';
			file.flush();
		}

		std::string sample(const char* label)
		{
			return timestamp() + " " + label + " map=" + map_name() + " " + process_memory() + " " +
				system_memory() + " " + gpu_memory() + " texture_failures=" + std::to_string(texture_failures.load());
		}

		void on_texture_failure(const d3d11::texture_diagnostics::event& value) noexcept
		{
			try
			{
				++texture_failures;
				char detail[160]{};
				std::snprintf(detail, sizeof(detail), " FAILED texture=%llux%llu mips=%llu format=%llu hresult=0x%08llx",
					static_cast<unsigned long long>(value.width), static_cast<unsigned long long>(value.height),
					static_cast<unsigned long long>(value.mips), static_cast<unsigned long long>(value.format),
					static_cast<unsigned long long>(value.result));
				append(sample("TEXTURE") + detail);
			}
			catch (...)
			{
			}
		}
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			enabled = dvars::register_bool("vr_memoryLog", true, game::DVAR_FLAG_SAVED,
				"Temporary: write minidumps/overlord-memory-log.txt every 5 seconds (capped at 1 MB)");
			append("");
			append(timestamp() + " ===== game started (" VERSION ") =====");
			d3d11::texture_diagnostics::set_failure_observer(on_texture_failure);
			scheduler::loop([]
			{
				if (alive) append(sample("sample"));
			}, scheduler::pipeline::async, interval);
			command::add("memory_log", []
			{
				const auto line = sample("manual");
				append(line);
				console::info("%s\n", line.data());
			});
		}

		void pre_destroy() override
		{
			alive = false;
			d3d11::texture_diagnostics::set_failure_observer(nullptr);
		}
	};
}

REGISTER_COMPONENT(memory_log::component)

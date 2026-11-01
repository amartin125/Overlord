#include <std_include.hpp>
#include "npc_collision.hpp"
#include "npc_collision_policy.hpp"
#include "native_scripted_control.hpp"
#include "weapon_carry_runtime.hpp"
#include "component/command.hpp"
#include "component/console.hpp"
#include "component/scheduler.hpp"
#include "game/dvars.hpp"
#include "game/scripting/array.hpp"
#include "game/scripting/entity.hpp"
#include "game/scripting/execution.hpp"
#include "loader/component_loader.hpp"
#include <utils/hook.hpp>
#include <utils/hook_validation.hpp>
#include <utils/io.hpp>

namespace vr::gameplay::npc_collision
{
	namespace
	{
		game::dvar_t* enabled{};
		game::dvar_t* pass_through_enabled{};
		std::atomic_uint64_t passed_through{},hostile_blocks{},hostile_refreshes{};
		actor_set hostiles;
		void refresh_hostiles()
		{
			if (!game::CL_IsCgameInitialized() || !*game::levelEntityId) {hostiles.clear();return;}
			try
			{
				const auto targets=scripting::call<scripting::array>("getaiarray",{"bad_guys"});
				const auto count=targets.size();
				if (count<0 || count>static_cast<int>(actor_set::capacity)) return;
				std::vector<unsigned> entities;entities.reserve(static_cast<std::size_t>(count));
				for (int i=0;i<count;++i)
				{
					const auto value=targets.get(i);
					if (!value.is<scripting::entity>()) continue;
					const auto ref=value.as<scripting::entity>().get_entity_reference();
					if (ref.classnum==0) entities.push_back(ref.entnum);
				}
				hostiles.publish(entities);
				++hostile_refreshes;
			}
			catch (...)
			{
				hostiles.clear();
			}
		}
		std::atomic_bool installed{};
		std::array<std::atomic_uint64_t,2> candidates{},adjustments{};
		std::atomic_uint64_t environment_blocks{},rejected{};
		constexpr std::uintptr_t native_trace=0x14068f210;
		template<class T> T read(const void* source,std::size_t offset) noexcept
		{T value{};utils::native_memory::read_bytes(&value,static_cast<const std::byte*>(source)+offset,sizeof(value));return value;}
		bool actor(const game::trace_t& trace) noexcept
		{
			if (!valid(trace) || !blocking(trace) || trace.hitType!=1 || !trace.hitId || trace.hitId>=3998 ||
				!(unsigned(trace.contents)&actor_contents)) return false;
			const auto* entity=&game::g_entities[trace.hitId];
			return read<std::uint8_t>(entity,0xbc) && read<void*>(entity,0x120) &&
				read<void*>(entity,0x128) && (read<unsigned>(entity,0xd8)&actor_contents);
		}
		void trace(unsigned char handler,game::trace_t* result,const float* start,const float* end,
			const game::Bounds* bounds,unsigned short skip,int mask,const game::playerState_s* ps)
		{
			const auto query=[&](game::trace_t* output,const game::Bounds* volume,int contents) {
				utils::hook::invoke<void>(native_trace,handler,output,start,end,volume,skip,contents,ps);
			};
			query(result,bounds,mask);
			// The three verified PM callers supply their player state. The shared
			// dispatcher preserves client (0) / server (1) native collision handlers.
			// Path-node synthetic movement (2) and all non-player traces stay native.
			if (!installed || !enabled || !enabled->current.enabled || !weapons::carry::active() || handler>1 || skip!=0 ||
				!ps || ps==reinterpret_cast<const game::playerState_s*>(&game::g_entities[0].client) ||
				!scripted_control::allowed(ps) || !bounds || !(unsigned(mask)&actor_contents) || !actor(*result)) return;
			++candidates[handler];
			const auto original=*result;
			game::trace_t contact{},environment{};
			const bool hostile=hostiles.contains(result->hitId);
			if (hostile) ++hostile_blocks;
			if (pass_through_enabled && pass_through_enabled->current.enabled && !hostile)
			{
				// Player movement ignores friendly/neutral NPC bodies but keeps every
				// wall, prop and enemy (enemies use the reduced-core policy below).
				query(&environment,bounds,mask&~int(actor_contents));
				*result=pass_through(original,environment);
				if (result->fraction!=original.fraction || result->startsolid!=original.startsolid) ++passed_through;
				return;
			}
			game::Bounds reduced=*bounds;if (!narrow(reduced)) return;
			// A smaller capsule alone would also weaken walls behind the NPC.
			// Retain a full-sized environment sweep and choose its earlier contact.
			query(&environment,bounds,mask&~int(actor_contents));
			query(&contact,&reduced,mask);
			if (!valid(environment) || !valid(contact) || (blocking(contact) && !actor(contact)))
			{++rejected;return;}
			*result=merge(original,environment,contact);
			if (blocking(environment) && (environment.startsolid || environment.allsolid || environment.fraction<=contact.fraction)) ++environment_blocks;
			if (result->fraction!=original.fraction || result->startsolid!=original.startsolid || result->allsolid!=original.allsolid) ++adjustments[handler];
		}
		template<std::size_t N> bool verify(std::uintptr_t address,const std::uint8_t (&bytes)[N])
		{
			std::array<std::uint8_t,N> mask{};mask.fill(255);
			return bool(utils::hook_validation::verify_masked_bytes(reinterpret_cast<void*>(address),{bytes,mask.data(),N}));
		}
	}
	std::string status()
	{
		return std::format("[VR NPC collision] installed={} enabled={} pass_through={} player_radius={} actor_candidates={}/{} adjusted={}/{} passed_through={} hostile_contacts={} hostile_refreshes={} environment_blocks={} rejected={}\n",
			installed.load(),enabled && enabled->current.enabled,pass_through_enabled && pass_through_enabled->current.enabled,player_radius,
			candidates[0].load(),candidates[1].load(),adjustments[0].load(),adjustments[1].load(),passed_through.load(),hostile_blocks.load(),
			hostile_refreshes.load(),environment_blocks.load(),rejected.load());
	}
	class component final:public component_interface
	{
		void post_unpack() override
		{
			enabled=dvars::register_bool("vr_closeNpcCollision",true,game::DVAR_FLAG_SAVED,"Closer player/NPC contact while retaining native environment clearance");
			pass_through_enabled=dvars::register_bool("vr_npcPassThrough",true,game::DVAR_FLAG_SAVED,
				"Let the player move through friendly and neutral NPC bodies (needs vr_closeNpcCollision); enemies, walls and props still block");
			scheduler::loop(refresh_hostiles,scheduler::pipeline::server,std::chrono::milliseconds(250));
			constexpr std::uint8_t entry[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
			constexpr std::uint8_t first[]{0xe8,0x0f,0x01,0,0},retry[]{0xe8,0x64,0,0,0},general[]{0xe8,0x09,0,0,0};
			// Read-only live witness: both PM_playerTrace calls and PM_trace pass
			// bounds/skip/mask/PS at stack slots 0x20/0x28/0x30/0x38 respectively.
			if (verify(native_trace,entry) && verify(0x14068f0fc,first) && verify(0x14068f1a7,retry) && verify(0x14068f202,general))
			{
				utils::hook::call(0x14068f0fc,trace);utils::hook::call(0x14068f1a7,trace);utils::hook::call(0x14068f202,trace);installed=true;
			}
			command::add("vr_npc_collision_status",[] {
				const auto report=status();console::print_text(console::con_type_info,report);
				scheduler::once([report]{utils::io::write_file("minidumps/overlord-npc-collision.txt",report);},scheduler::pipeline::async);
			});
		}
		void pre_destroy() override {installed=false;}
	};
}
REGISTER_COMPONENT(vr::gameplay::npc_collision::component)

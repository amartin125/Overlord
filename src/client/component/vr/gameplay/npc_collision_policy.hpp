#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace vr::gameplay::npc_collision
{
	inline constexpr unsigned actor_contents=0xc000;
	inline constexpr float player_radius=9.f;
	// Only horizontal player bounds change. Never enlarge a narrow stance or
	// alter floor/ceiling clearance, the NPC bounds, or a zero-sized ray.
	template<class Bounds> bool narrow(Bounds& bounds) noexcept
	{
		for (unsigned i=0;i<3;++i)
			if (!std::isfinite(bounds.midPoint[i]) || !std::isfinite(bounds.halfSize[i]) ||
				bounds.halfSize[i]<=0 || bounds.halfSize[i]>128.f) return false;
		if (bounds.halfSize[0]<=player_radius || bounds.halfSize[1]<=player_radius) return false;
		bounds.halfSize[0]=bounds.halfSize[1]=player_radius;
		return true;
	}
	template<class Trace> bool valid(const Trace& trace) noexcept
	{return std::isfinite(trace.fraction) && trace.fraction>=0 && trace.fraction<=1;}
	template<class Trace> bool blocking(const Trace& trace) noexcept
	{return trace.startsolid || trace.allsolid || trace.fraction<1;}
	// The full-sized environment sweep always wins on a tie or initial overlap.
	// A failed supplemental trace leaves the original native result intact.
	template<class Trace> Trace merge(const Trace& original,const Trace& environment,const Trace& contact) noexcept
	{
		if (!valid(original) || !valid(environment) || !valid(contact)) return original;
		if (environment.startsolid || environment.allsolid) return environment;
		if (contact.startsolid || contact.allsolid) return contact;
		return environment.fraction<=contact.fraction ? environment : contact;
	}
	// Optional pass-through: players reported both sides getting stuck once the
	// reduced core overlapped a friendly NPC in doorways and corridors. Use only
	// the full-sized environment sweep; a failed sweep keeps the native answer.
	template<class Trace> Trace pass_through(const Trace& original,const Trace& environment) noexcept
	{return valid(original) && valid(environment) ? environment : original;}
	// Hostile actors refreshed on the game thread and read lock-free by the
	// movement trace. Pass-through applies to friendlies and neutrals only, so
	// enemies keep blocking the player instead of being walked through.
	class actor_set
	{
		static constexpr unsigned words=63; // Entity numbers below 4032.
		std::array<std::atomic<std::uint64_t>,words> bits_{};
	public:
		static constexpr unsigned capacity=words*64;
		template<class Range> void publish(const Range& entities) noexcept
		{
			std::array<std::uint64_t,words> next{};
			for (const unsigned entity:entities) if (entity<capacity) next[entity/64]|=std::uint64_t{1}<<(entity%64);
			for (unsigned i=0;i<words;++i) bits_[i].store(next[i],std::memory_order_relaxed);
		}
		void clear() noexcept {for (auto& word:bits_) word.store(0,std::memory_order_relaxed);}
		[[nodiscard]] bool contains(unsigned entity) const noexcept
		{return entity<capacity && (bits_[entity/64].load(std::memory_order_relaxed)>>(entity%64)&1u);}
	};
}

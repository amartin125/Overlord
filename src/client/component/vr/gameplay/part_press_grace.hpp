#pragma once
#include <chrono>
#include <cstdint>

namespace vr::gameplay::weapons::physical_reload
{
	// A free-hand Trigger pressed shortly before the hand reaches a slide or
	// bolt handle still grabs it while that same press stays held. Only the
	// exact press event qualifies, so the arbiter still refuses a press that
	// another interaction already used. Magazines and waist draws keep their
	// fresh-press-in-place rule; a long hold never grabs anything on entry.
	inline constexpr float late_grab_radius_m=.04f; // Closer than the 11 cm press reach: arrive at the handle.
	inline constexpr int default_grace_ms=250,max_grace_ms=600;
	struct part_press_grace
	{
		using clock=std::chrono::steady_clock;
		std::uint64_t event{};
		clock::time_point until{};
		void press(std::uint64_t e,clock::time_point now,int grace_ms) noexcept
		{
			if(grace_ms<=0 || !e){*this={};return;}
			event=e;until=now+std::chrono::milliseconds(grace_ms>max_grace_ms?max_grace_ms:grace_ms);
		}
		bool late(std::uint64_t e,bool down,bool released,clock::time_point now) const noexcept
		{return event && e==event && down && !released && now<=until;}
		void clear() noexcept {*this={};}
	};
}

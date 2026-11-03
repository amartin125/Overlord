#pragma once
#include "weapon_carry.hpp"
#include <chrono>

namespace vr::gameplay::weapons::carry
{
	// An over-the-shoulder stow often opens Grip while the hand is already
	// coming forward again, a few centimetres outside the back slot. A held
	// weapon that passed through the slot moments earlier still stows there.
	inline constexpr std::chrono::milliseconds back_stow_grace{350};
	struct recent_back_reach
	{
		using clock = std::chrono::steady_clock;
		clock::time_point at{};
		bool seen{};
		void observe(location slot, clock::time_point now) noexcept
		{
			if (slot == location::back) { seen = true; at = now; }
		}
		void clear() noexcept { *this = {}; }
		location resolve(location slot, clock::time_point now) const noexcept
		{
			if (slot != location::absent || !seen || now < at || now - at > back_stow_grace) return slot;
			return location::back;
		}
	};
}

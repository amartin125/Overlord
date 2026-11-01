#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

namespace vr::comfort_vignette
{
	// Optional edge darkening while moving or turning with the sticks, a common
	// VR comfort aid. The command thread publishes a 0..1 target; the eye
	// transfer eases toward it and darkens only the outer field of view.
	inline std::atomic<float> target{};

	inline float motion_amount(const float forward, const float right, const bool turning) noexcept
	{
		const float stick = std::isfinite(forward) && std::isfinite(right) ? std::hypot(forward, right) : 0.f;
		return std::clamp(std::max(stick, turning ? 1.f : 0.f), 0.f, 1.f);
	}

	inline float target_for(const float strength, const float motion) noexcept
	{
		if (!std::isfinite(strength) || !std::isfinite(motion)) return 0.f;
		return std::clamp(strength, 0.f, 1.f) * std::clamp(motion, 0.f, 1.f);
	}

	// Fade in over about 0.15 s and out over about 0.35 s, so brief stick
	// releases do not flicker the edges.
	inline float approach(const float current, const float goal, const float seconds) noexcept
	{
		if (!std::isfinite(current) || !std::isfinite(goal) || !std::isfinite(seconds) || seconds <= 0)
			return std::isfinite(goal) ? std::clamp(goal, 0.f, 1.f) : 0.f;
		const float rate = goal > current ? 1.f / .15f : 1.f / .35f;
		const float step = std::min(seconds, .1f) * rate;
		return goal > current ? std::min(goal, current + step) : std::max(goal, current - step);
	}
}

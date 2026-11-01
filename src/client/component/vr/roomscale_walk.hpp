#pragma once

#include <array>
#include <cmath>

namespace vr::game_view
{
	// Optional physical walking. Native movement owns collision, so stepping in
	// the room is turned into a gentle movement request toward the head; the
	// leash then absorbs the body's catch-up so the view does not move twice.
	struct walk_axes
	{
		float forward{}, right{};
		[[nodiscard]] bool active() const noexcept { return forward != 0 || right != 0; }
	};

	// displacement: camera offset from the native eye in world metres (x, y).
	// yaw_degrees: native view yaw, which defines forwardmove/rightmove.
	// Speed ramps from zero at the deadzone to max_fraction at the leash radius.
	inline walk_axes walk_input(const std::array<float, 3>& displacement, const float yaw_degrees,
		const float deadzone, const float radius, const float max_fraction) noexcept
	{
		const float x = displacement[0], y = displacement[1];
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(yaw_degrees) || !std::isfinite(deadzone) ||
			!std::isfinite(radius) || !std::isfinite(max_fraction) || deadzone < 0 || radius <= deadzone ||
			max_fraction <= 0 || max_fraction > 1)
			return {};
		const float length = std::hypot(x, y);
		if (length <= deadzone) return {};
		const float speed = max_fraction * std::fmin((length - deadzone) / (radius - deadzone), 1.f);
		const float yaw = yaw_degrees * 0.017453292519943295f;
		const float fx = std::cos(yaw), fy = std::sin(yaw); // Native forward.
		const float rx = fy, ry = -fx;                      // Native right (+y is left).
		return {speed * (x * fx + y * fy) / length, speed * (x * rx + y * ry) / length};
	}
}

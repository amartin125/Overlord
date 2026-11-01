#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace vr::game_view
{
	// Native movement and collision stay with the player capsule while physical
	// head translation moves only the camera. Without a bound, stepping away from
	// the recenter point leaves the visible viewpoint far from the body that
	// collides, so players appear blocked by furniture they are not touching.
	// Keep the horizontal camera displacement within a radius of the native eye
	// by dragging a persistent world-space shift once the head passes it.
	class roomscale_leash
	{
	public:
		using vec = std::array<float, 3>;
		void reset() noexcept { *this = {}; }
		// displacement: world-space camera offset from the native eye, metres.
		// Returns the world-space shift to add to the camera and tracking origin.
		// Repeated calls with the same input are idempotent once inside the radius.
		vec apply(std::uint64_t reference, const vec& displacement, float radius) noexcept
		{
			if (!reference || !std::isfinite(radius) || radius <= 0.f ||
				!std::isfinite(displacement[0]) || !std::isfinite(displacement[1]))
			{
				reset();
				return {};
			}
			if (reference != reference_) { reset(); reference_ = reference; }
			const float x = displacement[0] + shift_[0], y = displacement[1] + shift_[1];
			const float length = std::sqrt(x * x + y * y);
			if (length > radius)
			{
				const float excess = 1.f - radius / length;
				shift_[0] -= x * excess;
				shift_[1] -= y * excess;
			}
			if (!std::isfinite(shift_[0]) || !std::isfinite(shift_[1])) { reset(); return {}; }
			return shift_;
		}
		// Physical walking moves the native body toward the camera. Remove that
		// body motion from the drag so the camera stays with the head. Only the
		// part of the motion toward the camera is absorbed, never more than the
		// current offset, so platforms, knockback or a blocked body cannot push
		// the view away from the head.
		void compensate(std::uint64_t reference, const vec& displacement, const vec& body_motion) noexcept
		{
			if (!reference || reference != reference_) return;
			for (const float value : {displacement[0], displacement[1], body_motion[0], body_motion[1]})
				if (!std::isfinite(value)) return;
			const float x = displacement[0] + shift_[0], y = displacement[1] + shift_[1];
			const float length = std::sqrt(x * x + y * y);
			if (length < 1e-4f) return;
			const float toward = (body_motion[0] * x + body_motion[1] * y) / length;
			if (toward <= 0) return;
			const float absorbed = toward < length ? toward : length;
			shift_[0] -= x / length * absorbed;
			shift_[1] -= y / length * absorbed;
		}
		[[nodiscard]] const vec& shift() const noexcept { return shift_; }
	private:
		vec shift_{};
		std::uint64_t reference_{};
	};
}

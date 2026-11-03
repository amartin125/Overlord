#pragma once
#include <chrono>

namespace vr::controller_profile
{
	// The legacy grip reference is read from SteamVR once, before OpenXR starts.
	// Controllers that were asleep or not yet identified at that moment leave
	// both gameplay hands unavailable even after they track. When a tracked grip
	// stays rejected by the reference, request a runtime reinitialization (which
	// repeats the query). Bounded per process so a broken runtime cannot loop.
	class reference_recovery
	{
	public:
		using clock = std::chrono::steady_clock;
		static constexpr auto settle = std::chrono::seconds(3);
		static constexpr auto retry_delay = std::chrono::seconds(20);
		static constexpr unsigned attempt_limit = 3;

		// Returns true when a reinitialization should be requested now.
		bool observe(bool rejected, clock::time_point now) noexcept
		{
			if (!rejected || (rejecting_ && now < since_))
			{
				rejecting_ = rejected;
				since_ = now;
				return false;
			}
			if (!rejecting_)
			{
				rejecting_ = true;
				since_ = now;
			}
			if (now - since_ < settle || attempts_ >= attempt_limit ||
			    (attempts_ && (now < last_ || now - last_ < retry_delay)))
				return false;
			++attempts_;
			last_ = now;
			return true;
		}
		unsigned attempts() const noexcept { return attempts_; }

	private:
		bool rejecting_{};
		unsigned attempts_{};
		clock::time_point since_{}, last_{};
	};
}

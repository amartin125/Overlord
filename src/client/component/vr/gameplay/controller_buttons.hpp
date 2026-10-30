#pragma once

#include "../controller_input.hpp"
#include "component/vr/digital_button_gate.hpp"
#include <cmath>

namespace vr::controller_input
{
	// H2 command bits verified through native key whitelist -> key dispatch ->
	// button packing, not copied from another CoD's usercmd definition.
	inline constexpr int sprint_button = 0x2;
	inline constexpr int jump_button = 0x400;

	struct button_commands
	{
		int held{};    // Native command bits, including a short tap between samples.
		int pressed{}; // Accepted new presses only, for native script notifications.
	};

	class command_buttons
	{
	public:
		button_commands consume(const frame& input, bool gameplay, clock::time_point now) noexcept
		{
			if (!gameplay || !input.focused || input.sequence == 0 || now < input.sampled_at ||
				now - input.sampled_at > std::chrono::milliseconds(150))
			{
				reset();
				return {};
			}
			if (input.reference_generation != reference_generation_ || input.sequence < sequence_ ||
				now < last_time_ || now - last_time_ > std::chrono::milliseconds(150)) reset();
			reference_generation_ = input.reference_generation;
			sequence_ = input.sequence;
			last_time_ = now;
			const int changed = (input.sprint.presses != sprint_.presses ? sprint_button : 0) |
				(input.jump.presses != jump_.presses ? jump_button : 0);
			int held = (sprint_.consume(input.sprint) ? sprint_button : 0) |
				(jump_.consume(input.jump) ? jump_button : 0);
			// Gamepad-style sprint: a stick click while pushing forward keeps the
			// native sprint request until forward input is released. Requiring the
			// click to stay held made sprint stop after a split second on Quest and
			// PSVR2 sticks, which also blocked the Cliffhanger gap jump.
			const float forward = input.move_active && std::isfinite(input.move[1]) ? input.move[1] : 0.f;
			// Like a gamepad, firing, reloading or grabbing with Trigger ends a
			// latched sprint; a new stick click is needed to sprint again.
			const bool trigger = (input.trigger[0].active && input.trigger[0].down) ||
				(input.trigger[1].active && input.trigger[1].down);
			if ((held & sprint_button) && forward >= sprint_latch_forward) sprint_latched_ = true;
			else if (forward < sprint_release_forward || trigger) sprint_latched_ = false;
			if (sprint_latched_) held |= sprint_button;
			// The level gates own admission/rearming for both outputs. A changed
			// counter from a rejected generation must never become a script event.
			return {held, held & changed};
		}

		void reset() noexcept
		{
			sprint_ = {};
			jump_ = {};
			sequence_ = 0;
			sprint_latched_ = false;
		}

	private:
		static constexpr float sprint_latch_forward = .5f, sprint_release_forward = .3f;
		digital_button_gate sprint_;
		digital_button_gate jump_;
		bool sprint_latched_{};
		std::uint64_t reference_generation_{};
		std::uint64_t sequence_{};
		clock::time_point last_time_{};
	};
}

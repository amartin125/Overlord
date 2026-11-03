#pragma once
#include "component/vr/controller_reference_recovery.hpp"

template<class Check> void controller_reference_recovery_tests(Check& check)
{
	using vr::controller_profile::reference_recovery;
	const reference_recovery::clock::time_point now{};
	const auto ms=[](int value){return std::chrono::milliseconds(value);};
	{
		reference_recovery recovery;
		check(!recovery.observe(false,now) && !recovery.observe(false,now+std::chrono::hours(1)),
			"identified controllers never request reinitialization");
		check(!recovery.observe(true,now) && !recovery.observe(true,now+reference_recovery::settle-ms(1)),
			"a brief grip rejection (recenter, profile change) does not reinitialize");
		check(recovery.observe(true,now+reference_recovery::settle),
			"a tracked grip the reference keeps rejecting requests reinitialization");
		check(!recovery.observe(true,now+reference_recovery::settle+ms(500)),
			"one rejection period requests one reinitialization");
		check(recovery.observe(true,now+reference_recovery::settle+reference_recovery::retry_delay),
			"a reinitialization that did not identify the controllers retries after the delay");
	}
	{
		reference_recovery recovery;
		(void)recovery.observe(true,now);
		(void)recovery.observe(false,now+ms(2500));
		check(!recovery.observe(true,now+reference_recovery::settle),
			"an accepted grip restarts the settle period");
	}
	{
		reference_recovery recovery;
		auto time=now;
		unsigned requests{};
		for(int step=0;step<2000;++step,time+=ms(500))
			requests+=recovery.observe(true,time);
		check(requests==reference_recovery::attempt_limit && recovery.attempts()==reference_recovery::attempt_limit,
			"a runtime that never identifies the controllers stops after the attempt limit");
		check(!recovery.observe(false,time) && !recovery.observe(true,time+std::chrono::hours(1)),
			"the attempt limit holds for the whole game session");
	}
	{
		reference_recovery recovery;
		(void)recovery.observe(true,now+std::chrono::seconds(10));
		check(!recovery.observe(true,now) && !recovery.observe(true,now+reference_recovery::settle-ms(1)),
			"a backwards clock restarts the settle period instead of reinitializing");
	}
}

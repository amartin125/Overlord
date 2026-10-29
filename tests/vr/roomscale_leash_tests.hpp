#pragma once
#include "component/vr/roomscale_leash.hpp"
#include <cmath>
#include <limits>

template<class Check> void roomscale_leash_tests(Check check)
{
	using vr::game_view::roomscale_leash;
	const auto camera=[](const roomscale_leash::vec& d,const roomscale_leash::vec& shift) {
		return std::hypot(d[0]+shift[0],d[1]+shift[1]);
	};
	{
		roomscale_leash leash;
		const auto shift=leash.apply(1,{.2f,0,0},.25f);
		check(shift[0]==0 && shift[1]==0,"head movement inside the radius stays 1:1");
		const roomscale_leash::vec far{1,.5f,.3f};
		const auto pulled=leash.apply(1,far,.25f);
		check(std::abs(camera(far,pulled)-.25f)<1e-4f,"stepping away from the body keeps the camera at the leash radius");
		check(pulled[2]==0,"the leash never changes camera height");
		const auto again=leash.apply(1,far,.25f);
		check(again==pulled,"repeated composition of one pose is idempotent");
		const auto back=leash.apply(1,{1.f,.5f,0},.25f);
		check(back==pulled,"small motion after dragging is again 1:1");
		const auto returned=leash.apply(1,{0,0,0},.25f);
		check(camera({0,0,0},returned)<=.2501f,"returning to the old spot drags the anchor back within range");
	}
	{
		roomscale_leash leash;leash.apply(1,{2,0,0},.25f);
		check(leash.apply(2,{0,0,0},.25f)==roomscale_leash::vec{},"a recenter clears the accumulated drag");
		leash.apply(2,{2,0,0},.25f);
		check(leash.apply(2,{2,0,0},0)==roomscale_leash::vec{},"zero radius disables the leash");
		check(leash.apply(0,{2,0,0},.25f)==roomscale_leash::vec{},"missing reference applies no shift");
		check(leash.apply(2,{std::numeric_limits<float>::quiet_NaN(),0,0},.25f)==roomscale_leash::vec{},"invalid displacement applies no shift");
	}
}

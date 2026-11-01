#pragma once
#include "component/vr/roomscale_leash.hpp"
#include "component/vr/roomscale_walk.hpp"
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
	{
		using vr::game_view::walk_input;
		check(!walk_input({.05f,0,0},0,.12f,.25f,.6f).active(),"small leans inside the deadzone never walk");
		const auto ahead=walk_input({.25f,0,0},0,.12f,.25f,.6f);
		check(std::abs(ahead.forward-.6f)<1e-4f && std::abs(ahead.right)<1e-4f,"a step straight ahead walks forward at the capped speed");
		const auto left=walk_input({0,.25f,0},0,.12f,.25f,.6f);
		check(std::abs(left.right+.6f)<1e-4f && std::abs(left.forward)<1e-4f,"a step to the left walks left (+y is left)");
		const auto turned=walk_input({0,.25f,0},90,.12f,.25f,.6f);
		check(std::abs(turned.forward-.6f)<1e-4f,"the step direction follows the native view yaw");
		const auto partial=walk_input({.185f,0,0},0,.12f,.25f,.6f);
		check(partial.forward>.29f && partial.forward<.31f,"speed ramps between the deadzone and the leash radius");
		check(!walk_input({.3f,0,0},0,.12f,0,.6f).active() && !walk_input({NAN,0,0},0,.12f,.25f,.6f).active(),
			"walking needs a valid leash radius and displacement");
	}
	{
		roomscale_leash leash;leash.apply(1,{0,0,0},.25f);
		const roomscale_leash::vec head{.2f,0,0};
		leash.compensate(1,head,{.05f,0,0});
		check(std::abs(camera(head,leash.shift())-.15f)<1e-4f,"body motion toward the head is absorbed so the view stays put");
		leash.compensate(1,head,{-.05f,0,0});
		check(std::abs(camera(head,leash.shift())-.15f)<1e-4f,"motion away from the head is never absorbed");
		leash.compensate(1,head,{1.f,0,0});
		check(camera(head,leash.shift())<1e-4f,"absorption never overshoots the current offset");
		leash.compensate(2,head,{.05f,0,0});
		check(camera(head,leash.shift())<1e-4f,"a stale reference cannot compensate");
	}
}

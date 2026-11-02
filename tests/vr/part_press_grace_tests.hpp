#pragma once
#include "component/vr/gameplay/part_press_grace.hpp"

template<class Check> void part_press_grace_tests(Check check)
{
	using namespace vr::gameplay::weapons::physical_reload;
	using namespace std::chrono_literals;
	const auto t=part_press_grace::clock::now();
	part_press_grace g;
	check(!g.late(0,true,false,t) && !g.late(7,true,false,t),"no remembered press never grabs");
	g.press(7,t,250);
	check(g.late(7,true,false,t+200ms),"same held press reaching the handle within the window grabs");
	check(!g.late(7,true,false,t+300ms),"a long hold never grabs on entry");
	check(!g.late(7,false,false,t+50ms) && !g.late(7,true,true,t+50ms),"released Trigger cancels the late grab");
	check(!g.late(8,true,false,t+50ms),"a different press event never inherits the window");
	g.press(9,t,0);
	check(!g.late(9,true,false,t),"grace 0 restores the fresh-press-in-place rule");
	g.press(10,t,5000);
	check(g.late(10,true,false,t+600ms) && !g.late(10,true,false,t+700ms),"the window is capped");
	g.clear();
	check(!g.late(10,true,false,t),"clear forgets the press");
}

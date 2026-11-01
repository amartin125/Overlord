#pragma once
#include "component/vr/comfort_vignette.hpp"
#include <limits>

template<class Check> void comfort_vignette_tests(Check check)
{
	using namespace vr::comfort_vignette;
	check(motion_amount(0,0,false)==0 && motion_amount(0,1,false)==1 && motion_amount(.3f,.4f,false)==.5f,
		"stick motion scales the vignette with deflection");
	check(motion_amount(0,0,true)==1,"turning applies the full vignette");
	check(target_for(0,1)==0 && target_for(.6f,1)==.6f && target_for(.6f,.5f)==.3f,"strength 0 disables and scales the target");
	check(target_for(std::numeric_limits<float>::quiet_NaN(),1)==0,"invalid strength never darkens");
	float v=0;for(int i=0;i<15;++i)v=approach(v,1,.01f);
	check(v>.99f,"fades in within about 0.15 s");
	for(int i=0;i<15;++i)v=approach(v,0,.01f);
	check(v>.5f,"fades out more slowly than it fades in");
	for(int i=0;i<25;++i)v=approach(v,0,.01f);
	check(v<.01f,"fully clears within about 0.35 s");
	check(approach(0,1,5)<=.67f,"a long frame gap cannot jump straight to full darkness");
}

#pragma once
#include "component/vr/engine_scene_extent.hpp"
#include <limits>

template<class Check> void render_scale_tests(Check check)
{
	using namespace vr::engine_scene_resolution;
	const extent quest{1832, 1920};
	check(scaled(quest, 1.f) == quest, "full render scale keeps the runtime recommendation");
	check(scaled(quest, .7f) == extent{1282, 1344}, "70 percent scale reduces both axes to even sizes");
	check(scaled(quest, .5f) == extent{916, 960}, "minimum scale halves the resolution");
	check(scaled(quest, .4f) == quest && scaled(quest, 1.5f) == quest, "out-of-range scales never change or enlarge the size");
	check(scaled(quest, std::numeric_limits<float>::quiet_NaN()) == quest, "invalid scale keeps the recommendation");
	check(scaled({3, 3}, .5f) == extent{2, 2}, "tiny sizes stay valid");
	check(scaled({}, .7f) == extent{}, "invalid runtime sizes are passed through for the existing rejection");
}

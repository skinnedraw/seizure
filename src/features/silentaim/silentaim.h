#pragma once
#include <sdk/sdk.h>
#include <cache/cache.h>

namespace rbx::silentaim
{
	// Track current target for sticky aim
	inline std::uint64_t sticky_target_address = 0;

	// Track last locked target for tracer drawing
	inline math::vector3 current_target_position{ 0, 0, 0 };
	inline bool has_target = false;

	void run();
	cache::entity_t* get_closest_player_near_mouse(float max_pixels);
}
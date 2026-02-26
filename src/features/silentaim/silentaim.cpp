#define NOMINMAX
#include <Windows.h>
#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <game/game.h>
#include <cache/cache.h>
#include <settings.h>
#include "silentaim.h"

namespace rbx::silentaim
{
	cache::entity_t* get_closest_player_near_mouse(float max_pixels)
	{
		POINT cursor;
		GetCursorPos(&cursor);
		ScreenToClient(game::wnd, &cursor);

		math::vector2 cursor_pos = { static_cast<float>(cursor.x), static_cast<float>(cursor.y) };

		cache::entity_t* closest_player = nullptr;
		float closest_distance = max_pixels;

		// Get viewport dimensions and view matrix once
		math::vector2 dims = game::visengine.get_dimensions();
		math::matrix4 view = game::visengine.get_viewmatrix();

		std::lock_guard<std::mutex> lock(cache::mtx);

		for (auto& player : cache::cached_players)
		{
			// Get head part
			auto head_it = player.parts.find("Head");
			if (head_it == player.parts.end())
				continue;

			// Get head position
			rbx::primitive_t head_prim = head_it->second.get_primitive();
			if (!head_prim.address)
				continue;

			math::vector3 head_pos = head_prim.get_position();
			math::vector2 head_screen;

			// Convert to screen space
			if (!game::visengine.world_to_screen(head_pos, head_screen, dims, view))
				continue;

			// Calculate distance from cursor
			math::vector2 diff = { head_screen.x - cursor_pos.x, head_screen.y - cursor_pos.y };
			float distance = diff.magnitude();

			if (distance < closest_distance)
			{
				closest_distance = distance;
				closest_player = &player;
			}
		}

		return closest_player;
	}

	void run()
	{
		// Set high priority for responsive silent aim
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

		static bool key_was_pressed = false;
		static bool toggle_state = false;

		// Get camera once at start
		rbx::camera_t camera = { game::workspace.find_first_child("Camera").address };
		if (!camera.address)
		{
			Sleep(1000);
			return;
		}

		std::uint64_t viewport_addr = camera.address + Offsets::Camera::Viewport;

		// Store default viewport
		math::vector2int16 default_viewport = memory->read<math::vector2int16>(viewport_addr);

		for (;;)
		{
			// Very tight loop - 5 microseconds = ~200k writes per second
			std::this_thread::sleep_for(std::chrono::microseconds(5));

			bool key_pressed = false;
			if (settings::silentaim::keybind_mode == 0) // Hold
			{
				key_pressed = GetAsyncKeyState(settings::silentaim::keybind) & 0x8000;

				// Reset sticky target when key released in hold mode
				if (!key_pressed && sticky_target_address != 0)
				{
					sticky_target_address = 0;
				}
			}
			else if (settings::silentaim::keybind_mode == 1) // Toggle
			{
				bool current_key_state = GetAsyncKeyState(settings::silentaim::keybind) & 0x8000;
				if (current_key_state && !key_was_pressed)
				{
					toggle_state = !toggle_state;
					key_was_pressed = true;

					// Reset sticky target when toggling off
					if (!toggle_state)
					{
						sticky_target_address = 0;
					}
				}
				else if (!current_key_state)
				{
					key_was_pressed = false;
				}
				key_pressed = toggle_state;
			}
			else if (settings::silentaim::keybind_mode == 2) // Always
			{
				key_pressed = true;
			}

			bool should_silent_aim = settings::silentaim::enabled && key_pressed;

			if (!should_silent_aim)
			{
				// Reset viewport to default
				memory->write<math::vector2int16>(viewport_addr, default_viewport);
				has_target = false;
				sticky_target_address = 0;
				continue;
			}

			// Get target player
			cache::entity_t* target = nullptr;

			if (settings::silentaim::sticky_aim && sticky_target_address != 0)
			{
				// Try to find the sticky target in cache
				std::lock_guard<std::mutex> lock(cache::mtx);
				for (auto& player : cache::cached_players)
				{
					if (player.instance.address == sticky_target_address)
					{
						target = &player;
						break;
					}
				}

				// If sticky target not found, reset it
				if (!target)
				{
					sticky_target_address = 0;
				}
			}

			// If no sticky target or sticky disabled, find closest to mouse
			if (!target)
			{
				target = get_closest_player_near_mouse(settings::silentaim::fov);

				// Set as sticky target if sticky aim is enabled
				if (target && settings::silentaim::sticky_aim)
				{
					sticky_target_address = target->instance.address;
				}
			}

			if (!target)
			{
				// No target found - reset viewport
				memory->write<math::vector2int16>(viewport_addr, default_viewport);
				has_target = false;
				continue;
			}

			// Get head part
			auto head_it = target->parts.find("Head");
			if (head_it == target->parts.end())
			{
				has_target = false;
				continue;
			}

			rbx::primitive_t head_prim = head_it->second.get_primitive();
			if (!head_prim.address)
			{
				has_target = false;
				continue;
			}

			math::vector3 head_pos = head_prim.get_position();

			// Apply prediction if enabled
			if (settings::silentaim::use_prediction)
			{
				math::vector3 velocity = memory->read<math::vector3>(head_prim.address + Offsets::Primitive::AssemblyLinearVelocity);

				head_pos.x += velocity.x * settings::silentaim::prediction_time;
				head_pos.y += velocity.y * settings::silentaim::prediction_time;
				head_pos.z += velocity.z * settings::silentaim::prediction_time;
			}

			// Store for tracer drawing
			current_target_position = head_pos;
			has_target = true;

			// Get viewport dimensions and view matrix
			math::vector2 dims = game::visengine.get_dimensions();
			math::matrix4 view = game::visengine.get_viewmatrix();

			// Convert predicted position to screen space
			math::vector2 target_screen;
			if (!game::visengine.world_to_screen(head_pos, target_screen, dims, view))
			{
				has_target = false;
				continue;
			}

			// Calculate viewport offset (this is the magic)
			math::vector2int16 viewport_offset;
			viewport_offset.x = static_cast<std::int16_t>(2 * (dims.x - target_screen.x));
			viewport_offset.y = static_cast<std::int16_t>(2 * (dims.y - target_screen.y));

			// Spam write viewport offset (5 writes per iteration)
			memory->write<math::vector2int16>(viewport_addr, viewport_offset);
			memory->write<math::vector2int16>(viewport_addr, viewport_offset);
			memory->write<math::vector2int16>(viewport_addr, viewport_offset);
			memory->write<math::vector2int16>(viewport_addr, viewport_offset);
			memory->write<math::vector2int16>(viewport_addr, viewport_offset);
		}
	}
}
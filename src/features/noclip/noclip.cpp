#define NOMINMAX
#include <Windows.h>
#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <game/game.h>
#include <settings.h>
#include "noclip.h"

namespace noclip
{
	struct cached_primitive_t
	{
		std::uint64_t address;
		std::uint8_t original_flags;
	};

	void run()
	{
		static bool key_was_pressed = false;
		static bool toggle_state = false;
		static std::vector<cached_primitive_t> cached_primitives;
		static std::uint64_t cached_model_address = 0;
		static int failed_cache_attempts = 0;

		for (;;)
		{
			Sleep(16);

			// Keybind handling
			bool key_pressed = false;
			if (settings::noclip::keybind_mode == 0) // Hold mode
			{
				key_pressed = GetAsyncKeyState(settings::noclip::keybind) & 0x8000;
			}
			else if (settings::noclip::keybind_mode == 1) // Toggle mode
			{
				bool current_key_state = GetAsyncKeyState(settings::noclip::keybind) & 0x8000;
				if (current_key_state && !key_was_pressed)
				{
					toggle_state = !toggle_state;
					key_was_pressed = true;
				}
				else if (!current_key_state)
				{
					key_was_pressed = false;
				}
				key_pressed = toggle_state;
			}
			else if (settings::noclip::keybind_mode == 2) // Always on
			{
				key_pressed = true;
			}

			bool should_noclip = settings::noclip::enabled && key_pressed;

			// Wait for local player to load
			if (game::local_player.address == 0)
			{
				if (cached_model_address != 0)
				{
					cached_primitives.clear();
					cached_model_address = 0;
					failed_cache_attempts = 0;
				}
				continue;
			}

			rbx::player_t local_player_obj = { game::local_player.address };
			rbx::model_instance_t model_instance = local_player_obj.get_model_instance();

			// Character is gone (resetting/died) - invalidate cache and wait
			if (model_instance.address == 0)
			{
				if (cached_model_address != 0)
				{
					cached_primitives.clear();
					cached_model_address = 0;
					failed_cache_attempts = 0;
				}
				continue;
			}

			// New character loaded - rebuild primitive cache
			if (model_instance.address != cached_model_address)
			{
				cached_primitives.clear();
				cached_model_address = model_instance.address;
				failed_cache_attempts = 0;

				// Wait longer for character to fully load and physics to initialize
				// Roblox physics runs at 60Hz (16.67ms) so wait several frames
				Sleep(1000);

				// Build primitive cache with validation
				std::vector<cached_primitive_t> temp_primitives;
				for (auto& part : model_instance.get_children<rbx::part_t>())
				{
					std::string class_name = part.get_class_name();
					if (class_name.find("Part") == std::string::npos)
						continue;

					rbx::primitive_t prim = part.get_primitive();
					if (prim.address == 0)
						continue;

					// Read and cache the ORIGINAL flags
					std::uint8_t original_flags = memory->read<std::uint8_t>(prim.address + Offsets::Primitive::Flags);

					// Only cache if primitive is valid
					if (original_flags != 0 && original_flags != 0xFF)
					{
						cached_primitive_t cached_prim;
						cached_prim.address = prim.address;
						cached_prim.original_flags = original_flags;
						temp_primitives.push_back(cached_prim);
					}
				}

				// Only accept the cache if we got at least some parts (should have 6+ for R6, 15+ for R15)
				if (temp_primitives.size() >= 5)
				{
					cached_primitives = temp_primitives;
					failed_cache_attempts = 0;
				}
				else
				{
					failed_cache_attempts++;
					// If we failed multiple times, wait longer before next attempt
					if (failed_cache_attempts > 3)
					{
						Sleep(2000);
					}
					cached_model_address = 0; // Force retry on next iteration
				}
			}

			// No primitives cached yet
			if (cached_primitives.empty())
				continue;

			// Validate primitives are still valid before applying noclip
			bool cache_invalid = false;
			for (const auto& cached_prim : cached_primitives)
			{
				// Try to read flags - if this fails, the primitive is invalid
				std::uint8_t test_flags = memory->read<std::uint8_t>(cached_prim.address + Offsets::Primitive::Flags);
				if (test_flags == 0 || test_flags == 0xFF)
				{
					cache_invalid = true;
					break;
				}
			}

			// If cache is invalid, force rebuild
			if (cache_invalid)
			{
				cached_primitives.clear();
				cached_model_address = 0;
				continue;
			}

			// Apply noclip to all cached primitives
			for (const auto& cached_prim : cached_primitives)
			{
				if (should_noclip)
				{
					// Disable CanCollide flag
					std::uint8_t current_flags = memory->read<std::uint8_t>(cached_prim.address + Offsets::Primitive::Flags);
					memory->write<std::uint8_t>(cached_prim.address + Offsets::Primitive::Flags, current_flags & ~Offsets::PrimitiveFlags::CanCollide);
				}
				else
				{
					// Restore original flags
					memory->write<std::uint8_t>(cached_prim.address + Offsets::Primitive::Flags, cached_prim.original_flags);
				}
			}
		}
	}
}
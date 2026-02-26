#pragma once
#include "keybind/keybind.h"

namespace settings
{
	namespace visuals
	{
		inline bool box{ false };
		inline float box_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool name{ false };
		inline float name_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool distance{ false };
		inline float distance_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool tool{ false };
		inline float tool_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool localplayer{ false };
	}

	namespace aimbot
	{
		inline bool enabled{ false };
		inline int aim_type{ 0 };  // 0 = Camera, 1 = Mouse
		inline int aim_part{ 0 };  // 0 = Head, 1 = Torso (UpperTorso/Torso), 2 = HumanoidRootPart
		inline int keybind{ 0 };
		inline int keybind_mode{ 0 };
		inline float fov{ 100.0f };
		inline bool show_fov{ false };
		inline float fov_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline bool sticky_aim{ false };
		inline float smoothing{ 1.0f };  // 0.1 = very smooth, 1.0 = instant
	}

	namespace silentaim
	{
		inline bool enabled{ false };
		inline int keybind{ 0 };
		inline int keybind_mode{ 0 };
		inline float fov{ 150.0f }; // Separate FOV from aimbot
		inline bool show_fov{ false };
		inline float fov_color[4]{ 1.f, 0.5f, 0.f, 1.f }; // Orange by default
		inline bool sticky_aim{ false };
		inline bool show_tracer{ false };
		inline float tracer_color[4]{ 1.f, 0.f, 0.f, 1.f }; // Red by default
		inline bool use_prediction{ true };
		inline float prediction_time{ 0.090f }; // 90ms
	}

	namespace walkspeed
	{
		inline bool enabled{ false };
		inline float speed{ 16.0f };
		inline int keybind{ 0 };
		inline int keybind_mode{ 0 };
	}

	namespace noclip
	{
		inline bool enabled{ false };
		inline int keybind{ 0 };
		inline int keybind_mode{ 0 };
	}

	namespace freezepos
	{
		inline bool enabled{ false };
		inline int keybind{ 0 };
		inline int keybind_mode{ 0 };
	}

	namespace voidhide
	{
		inline bool enabled{ false };
		inline int keybind{ 0 };
		inline int keybind_mode{ 0 };
	}

	namespace settings
	{
		inline bool hide_console{ false };
		inline bool should_unload{ false };
		inline bool should_update_offsets{ false };
		inline int menu_key{ 'P' };
	}
}
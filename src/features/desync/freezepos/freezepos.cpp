#define NOMINMAX
#include <Windows.h>
#include <chrono>
#include <memory/memory.h>
#include <sdk/offsets.h>
#include <settings.h>
#include "freezepos.h"

namespace freezepos
{
    void run()
    {
        static bool key_was_pressed = false;
        static bool toggle_state = false;
        static bool was_enabled = false;
        static int original_value = -1;
        static bool is_restoring = false;
        static bool has_been_used = false;
        static auto restore_start_time = std::chrono::steady_clock::now();

        for (;;)
        {
            Sleep(1);

            // Keybind logic
            bool key_pressed = false;
            if (settings::freezepos::keybind_mode == 0) // Hold
            {
                key_pressed = GetAsyncKeyState(settings::freezepos::keybind) & 0x8000;
            }
            else if (settings::freezepos::keybind_mode == 1) // Toggle
            {
                bool current = GetAsyncKeyState(settings::freezepos::keybind) & 0x8000;
                if (current && !key_was_pressed)
                {
                    toggle_state = !toggle_state;
                    key_was_pressed = true;
                }
                else if (!current)
                {
                    key_was_pressed = false;
                }
                key_pressed = toggle_state;
            }
            else if (settings::freezepos::keybind_mode == 2) // Always
            {
                key_pressed = true;
            }

            bool currently_enabled = settings::freezepos::enabled && key_pressed;

            uintptr_t base = memory->get_module_address() + StaticOffsets::PhysicsSenderMaxBandwidthBps;

            if (currently_enabled && !was_enabled)
            {
                // Just turned on: save original then zero it
                if (original_value == -1)
                {
                    original_value = memory->read<int>(base);
                    has_been_used = true;
                }
                memory->write<int>(base, 0);
                is_restoring = false;
            }
            else if (!currently_enabled && was_enabled)
            {
                // Just turned off: start restore timer
                if (original_value != -1)
                {
                    restore_start_time = std::chrono::steady_clock::now();
                    is_restoring = true;
                }
            }
            else if (currently_enabled)
            {
                // Still on: keep writing 0
                memory->write<int>(base, 0);
            }

            // Restore for 3 seconds after disabling
            if (is_restoring && original_value != -1)
            {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - restore_start_time);

                if (elapsed.count() < 3000)
                {
                    memory->write<int>(base, original_value);
                }
                else
                {
                    is_restoring = false;
                    original_value = -1;
                }
            }

            was_enabled = currently_enabled;
        }
    }
}
#define NOMINMAX
#include <cstdint>
#include <chrono>
#include <thread>
#include <Windows.h>
#include <TlHelp32.h>

#include <memory/memory.h>
#include <sdk/offsets.h>
#include <sdk/sdk.h>
#include <game/game.h>
#include <cache/cache.h>
#include <render/render.h>
#include <features/aimbot/aimbot.h>
#include <features/walkspeed/walkspeed.h>
#include <features/desync/freezepos/freezepos.h>
#include <features/noclip/noclip.h>
#include <teleport/teleport_handler.h>
#include <settings.h>

std::int32_t main()
{
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	GetConsoleMode(hOut, &dwMode);
	SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mWelcome to seizure \x1b[0;38;5;67;49mnightly \x1b[38;2;169;169;169m0.0.0\x1b[0m\n");

	printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mFetching \x1b[38;2;169;169;169moffsets...\x1b[0m\n");
	if (!Offsets::FetchAndLoad())
	{
		printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mFailed to fetch offsets. \x1b[38;2;169;169;169mCheck your internet connection.\x1b[0m\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}
	printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mLoaded \x1b[38;2;169;169;169m%zu\x1b[38;2;255;255;255m offsets for version \x1b[38;2;169;169;169m%s\x1b[0m\n",
		Offsets::Map.size(), Offsets::ClientVersion.c_str());

	static const char* BINARY_NAME = "RobloxPlayerBeta.exe";

	if (!memory->find_process_id(BINARY_NAME))
	{
		printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mWaiting for you to open \x1b[38;2;169;169;169m%s\x1b[0m\n", BINARY_NAME);
		while (!memory->find_process_id(BINARY_NAME))
			std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	auto start_time = std::chrono::high_resolution_clock::now();

	printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mFound %s \x1b[38;2;169;169;169m@ %u\x1b[0m\n", BINARY_NAME, memory->get_process_id());

	if (!memory->attach_to_process(BINARY_NAME))
	{
		printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mUnable to attach to \x1b[38;2;169;169;169m%s\x1b[0m\n", BINARY_NAME);
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}

	printf("\n");
	printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mAttempting to \x1b[38;2;169;169;169mfind addresses...\x1b[0m\n");

	if (!memory->find_module_address(BINARY_NAME))
	{
		printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mUnable to find \x1b[38;2;169;169;169mmain module address\x1b[0m\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}

	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mFound base \x1b[38;2;169;169;169m@ 0x%llx\x1b[0m\n", memory->get_module_address());

	// Detect running Roblox version and warn if it differs from offset version
	std::string version = "unknown";
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, memory->get_process_id());
	if (snapshot != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32W me = { sizeof(MODULEENTRY32W) };
		if (Module32FirstW(snapshot, &me))
		{
			std::wstring path(me.szExePath);
			std::string narrow_path(path.begin(), path.end());

			size_t pos = narrow_path.find("version-");
			if (pos != std::string::npos && pos + 24 <= narrow_path.length())
				version = narrow_path.substr(pos, 24);
		}
		CloseHandle(snapshot);
	}

	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mDetected version \x1b[38;2;169;169;169m- %s\x1b[0m\n", version.c_str());
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mOffset  version  \x1b[38;2;169;169;169m- %s\x1b[0m\n", Offsets::ClientVersion.c_str());

	if (version != "unknown" && version != Offsets::ClientVersion)
	{
		printf("\x1b[38;2;255;100;100m- - WARNING: version mismatch! Offsets may be outdated.\x1b[0m\n");
	}

	std::uint64_t fake_datamodel{ memory->read<std::uint64_t>(memory->get_module_address() + OFF(FakeDataModel, Pointer)) };
	game::datamodel = rbx::instance_t(memory->read<std::uint64_t>(fake_datamodel + OFF(FakeDataModel, RealDataModel)));

	if (game::datamodel.get_name() != "Ugc")
	{
		printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mWaiting for \x1b[38;2;169;169;169mdatamodel to load...\x1b[0m\n");
		while (game::datamodel.get_name() != "Ugc")
		{
			std::uint64_t fake_dm{ memory->read<std::uint64_t>(memory->get_module_address() + OFF(FakeDataModel, Pointer)) };
			game::datamodel = rbx::instance_t(memory->read<std::uint64_t>(fake_dm + OFF(FakeDataModel, RealDataModel)));
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}

	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mFound datamodel \x1b[38;2;169;169;169m@ 0x%llx (%s)\x1b[0m\n", game::datamodel.address, game::datamodel.get_name().c_str());

	game::visengine = { memory->read<std::uint64_t>(memory->get_module_address() + OFF(VisualEngine, Pointer)) };
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mFound visualengine \x1b[38;2;169;169;169m@ 0x%llx\x1b[0m\n", game::visengine.address);

	game::workspace = { game::datamodel.find_first_child_by_class("Workspace") };
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mFound workspace \x1b[38;2;169;169;169m@ 0x%llx\x1b[0m\n", game::workspace.address);

	game::players = { game::datamodel.find_first_child_by_class("Players") };
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mFound players \x1b[38;2;169;169;169m@ 0x%llx\x1b[0m\n", game::players.address);

	if (game::local_player.address == 0)
	{
		printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mWaiting for local_player \x1b[38;2;169;169;169mto load...\x1b[0m\n");
		while (game::local_player.address == 0)
		{
			game::local_player = { memory->read<std::uint64_t>(game::players.address + OFF(Player, LocalPlayer)) };
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}

	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mFound local_player \x1b[38;2;169;169;169m@ 0x%llx\x1b[0m\n", game::local_player.address);

	printf("\n");
	game::detect_game();

	game::wnd = FindWindowA(nullptr, "Roblox");

	printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mFound \x1b[38;2;169;169;169mRoblox window\x1b[0m\n");

	printf("\n");
	printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mAttempting to \x1b[38;2;169;169;169mstart threads...\x1b[0m\n");
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mRunning \x1b[38;2;169;169;169mteleport_handler.h\x1b[0m\n");
	std::thread(teleport::run).detach();
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mRunning \x1b[38;2;169;169;169mcache.h\x1b[0m\n");
	std::thread(cache::run).detach();
	rbx::aimbot::initialize();
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mRunning \x1b[38;2;169;169;169maimbot.h\x1b[0m\n");
	std::thread(rbx::aimbot::run).detach();
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mRunning \x1b[38;2;169;169;169mwalkspeed.h\x1b[0m\n");
	std::thread(walkspeed::run).detach();
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mRunning \x1b[38;2;169;169;169mfreezepos.h\x1b[0m\n");
	std::thread(freezepos::run).detach();
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mExcluded \x1b[38;2;169;169;169mnoclip.h\x1b[0m\n");
	// std::thread(noclip::run).detach();

	printf("\n");
	printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mAttempting to \x1b[38;2;169;169;169mstart overlay\x1b[0m\n");
	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mCreating \x1b[38;2;169;169;169mwindow\x1b[0m\n");
	if (!render->create_window())
	{
		printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mFailed to \x1b[38;2;169;169;169mcreate window\x1b[0m\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}

	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mCreating \x1b[38;2;169;169;169mdevice\x1b[0m\n");
	if (!render->create_device())
	{
		printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mFailed to \x1b[38;2;169;169;169mcreate device\x1b[0m\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}

	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mCreating \x1b[38;2;169;169;169mimgui\x1b[0m\n");
	if (!render->create_imgui())
	{
		printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mFailed to \x1b[38;2;169;169;169mcreate imgui\x1b[0m\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}

	printf("\x1b[38;2;169;169;169m- - \x1b[38;2;255;255;255mStarted \x1b[38;2;169;169;169moverlay\x1b[0m\n");
	printf("\n");
	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration<double>(end_time - start_time).count();
	printf("\x1b[38;2;169;169;169m- \x1b[38;2;255;255;255mseizure \x1b[0;38;5;67;49mnightly \x1b[38;2;169;169;169mfinished loading & took %.2fs\x1b[0m\n", duration);
	printf("\x1b[38;2;169;169;169m - Press P to open the menu\x1b[0m\n\n");

	while (true)
	{
		if (settings::settings::should_unload)
			break;

		render->start_render();
		render->render_visuals();

		if (render->running)
			render->render_menu();

		render->end_render();
	}

	return 0;
}
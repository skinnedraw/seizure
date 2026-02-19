#include "teleport_handler.h"
#include <thread>
#include <chrono>
#include <memory/memory.h>
#include <sdk/offsets.h>
#include <sdk/sdk.h>
#include <game/game.h>
#include <cache/cache.h>

namespace teleport
{
	void run()
	{
		std::uint64_t last_place_id = memory->read<std::uint64_t>(game::datamodel.address + OFF(DataModel, PlaceId));
		std::string   last_job_id = memory->read_string(game::datamodel.address + OFF(DataModel, JobId));

		while (true)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(250));

			std::uint64_t fake_dm = memory->read<std::uint64_t>(memory->get_module_address() + OFF(FakeDataModel, Pointer));
			rbx::instance_t dm(memory->read<std::uint64_t>(fake_dm + OFF(FakeDataModel, RealDataModel)));

			if (!dm.address || dm.get_name() != "Ugc")
			{
				last_place_id = 0;
				last_job_id = "";
				continue;
			}

			std::uint64_t place_id = memory->read<std::uint64_t>(dm.address + OFF(DataModel, PlaceId));
			std::string   job_id = memory->read_string(dm.address + OFF(DataModel, JobId));

			if ((place_id != last_place_id || job_id != last_job_id) && place_id != 0)
			{
				game::datamodel = dm;
				game::visengine = { memory->read<std::uint64_t>(memory->get_module_address() + OFF(VisualEngine, Pointer)) };
				game::workspace = { game::datamodel.find_first_child_by_class("Workspace") };
				game::players = { game::datamodel.find_first_child_by_class("Players") };
				game::local_player = { memory->read<std::uint64_t>(game::players.address + OFF(Player, LocalPlayer)) };

				// Clear cache
				{
					std::lock_guard<std::mutex> lock(cache::mtx);
					cache::cached_players.clear();
					cache::cached_local_player = {};
				}

				// Wait for cache to rebuild with the correct local player
				int attempts = 0;
				while (attempts < 50) // max 5 seconds
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));

					bool cache_ready = false;
					{
						std::lock_guard<std::mutex> lock(cache::mtx);
						cache_ready = (cache::cached_local_player.instance.address == game::local_player.address)
							&& (cache::cached_local_player.instance.address != 0);
					}

					if (cache_ready)
						break;

					attempts++;
				}

				last_place_id = place_id;
				last_job_id = job_id;
			}
		}
	}
}
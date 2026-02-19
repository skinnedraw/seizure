#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <stdexcept>

namespace Offsets {
    // Populated at startup by FetchAndLoad()
    inline std::unordered_map<std::string, uintptr_t> Map;
    inline std::string ClientVersion;

    // Downloads https://imtheo.lol/Offsets/Offsets.hpp and parses it into Map.
    // Returns true on success. Call this once before anything else.
    bool FetchAndLoad(const std::string& url = "https://imtheo.lol/Offsets/Offsets.hpp");

    // Throws std::runtime_error if the key is missing — catches typos fast.
    inline uintptr_t Get(const std::string& ns, const std::string& name) {
        auto key = ns + "::" + name;
        auto it = Map.find(key);
        if (it == Map.end())
            throw std::runtime_error("[offsets] missing key: " + key);
        return it->second;
    }
}

// Convenience macro so call-sites read naturally: OFF(Humanoid, Walkspeed)
#define OFF(ns, name) Offsets::Get(#ns, #name)

// ---------------------------------------------------------------------------
// Static offsets — these are hardcoded and not pulled from the remote source.
// Add offsets here that you want to manage manually.
// ---------------------------------------------------------------------------
namespace StaticOffsets
{
    // DFInt S2PhysicsSenderRate — controls how often physics data is sent to the server.
    // Set to 0 to effectively freeze/suppress sends.
    inline constexpr uintptr_t PhysicsSenderMaxBandwidthBps = 0x69f6e10;
}
#define NOMINMAX
#include <Windows.h>
#include <winhttp.h>
#include <sdk/offsets.h>
#include <regex>
#include <sstream>
#include <string>
#pragma comment(lib, "winhttp.lib")

// -----------------------------------------------------------------------
// Internal: blocking HTTPS GET via WinHTTP (no extra dependencies)
// -----------------------------------------------------------------------
static std::string HttpGet(const std::wstring& host, const std::wstring& path)
{
    std::string result;

    HINTERNET hSession = WinHttpOpen(
        L"seizure-offset-fetcher/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) return result;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return result; }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(hRequest, nullptr))
    {
        DWORD bytesRead = 0;
        char buf[4096];
        while (WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead)
            result.append(buf, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

// -----------------------------------------------------------------------
// Public: fetch remote .hpp and populate Offsets::Map
// -----------------------------------------------------------------------
bool Offsets::FetchAndLoad(const std::string& /*url*/)
{
    // Host and path are split out; url param is kept for API compatibility.
    std::string content = HttpGet(L"imtheo.lol", L"/Offsets/Offsets.hpp");
    if (content.empty())
        return false;

    Map.clear();

    // --- ClientVersion ---------------------------------------------------
    // Match:  ClientVersion = "version-xxxxx"
    {
        std::regex verRe("ClientVersion\\s*=\\s*\"([^\"]+)\"");
        std::smatch m;
        if (std::regex_search(content, m, verRe))
            ClientVersion = m[1].str();
    }

    // --- Offset entries --------------------------------------------------
    // Walk line by line tracking the innermost non-"Offsets" namespace.
    // namespace Foo {
    //   inline constexpr uintptr_t Bar = 0x1A2;
    // }
    std::regex nsRe("^\\s*namespace\\s+(\\w+)\\s*\\{");
    std::regex offRe("inline\\s+constexpr\\s+uintptr_t\\s+(\\w+)\\s*=\\s*(0x[0-9a-fA-F]+)");
    std::smatch m;

    // Stack to handle nested namespaces (Offsets { ... Humanoid { ... } })
    // We only record the *innermost* non-"Offsets" namespace as the key prefix.
    std::string currentNs;
    std::istringstream ss(content);
    std::string line;

    while (std::getline(ss, line))
    {
        if (std::regex_search(line, m, nsRe))
        {
            std::string ns = m[1].str();
            if (ns != "Offsets")          // skip the outer wrapper namespace
                currentNs = ns;
        }
        else if (!currentNs.empty() && std::regex_search(line, m, offRe))
        {
            std::string key = currentNs + "::" + m[1].str();
            uintptr_t   val = static_cast<uintptr_t>(std::stoull(m[2].str(), nullptr, 16));
            Map[key] = val;
        }
        else if (line.find('}') != std::string::npos)
        {
            // Closing brace — pop back to "no namespace" so the next
            // `namespace X {` line sets a fresh one.
            currentNs.clear();
        }
    }

    return !Map.empty();
}
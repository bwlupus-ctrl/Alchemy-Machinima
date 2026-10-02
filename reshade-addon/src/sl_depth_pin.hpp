#pragma once

// [DepthPin] Private GD ABI: ONLY the exact reviewed binary may reach these fields.
// https://github.com/crosire/reshade/blob/v6.8.0/examples/09-depth/generic_depth_addon.cpp
// GD retains ownership of views, backup copies, DEPTH and bufready_depth.
#include <Windows.h>
#include <bcrypt.h>
#include "reshade.hpp"
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <system_error>
#include <vector>

namespace depth_pin
{
using namespace reshade::api;
static constexpr uint64_t window_depth = 0x008218000000821Aull;
static constexpr GUID runtime_guid =
    { 0x7c6363c7, 0xf94e, 0x437a, { 0x91, 0x60, 0x14, 0x17, 0x82, 0xc4, 0x4a, 0x98 } };
struct Prefix { uint64_t selected, override_depth, srv; };
static_assert(offsetof(Prefix, override_depth) == 8 && offsetof(Prefix, srv) == 16 && sizeof(Prefix) == 24);
static bool binary_allowed = false;

// [DepthPin] Called once from runtime init, outside DllMain / the loader lock.
static void check_binary()
{
    const HMODULE module = reshade::internal::get_reshade_module_handle();
    wchar_t path[32768] = {};
    const DWORD length = GetModuleFileNameW(module, path, 32768);
    auto allowed = [&]() -> bool {
        if (sizeof(void *) != 8 || !module || !length || length >= 32768)
            return false;
        DWORD ignored = 0;
        const DWORD size = GetFileVersionInfoSizeW(path, &ignored);
        if (!size) return false;
        std::vector<unsigned char> version(size);
        if (!GetFileVersionInfoW(path, 0, size, version.data())) return false;
        VS_FIXEDFILEINFO *info = nullptr;
        UINT info_size = 0;
        if (!VerQueryValueW(version.data(), L"\\", reinterpret_cast<void **>(&info), &info_size) ||
            info_size < sizeof(*info) || info->dwSignature != 0xfeef04bd ||
            info->dwFileVersionMS != MAKELONG(8, 6) || info->dwFileVersionLS != MAKELONG(2155, 0))
            return false;
        // Deny concurrent writes/replacement while hashing the actual exporting module's file.
        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        unsigned char digest[32] = {};
        bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
        if (ok) ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
        unsigned char buffer[16384];
        while (ok)
        {
            DWORD bytes = 0;
            if (!ReadFile(file, buffer, sizeof(buffer), &bytes, nullptr)) { ok = false; break; }
            if (!bytes) break;
            ok = BCryptHashData(hash, buffer, bytes, 0) >= 0;
        }
        if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        CloseHandle(file);
        static constexpr unsigned char expected[32] = {
            0x0c,0xee,0x63,0xf9,0xc9,0xf1,0x3f,0x3a,0xc9,0x09,0xc5,0xb4,0x90,0x3f,0x4d,0xbb,
            0x4b,0x71,0x9a,0x7a,0xb3,0xb4,0xf1,0x3b,0x0d,0xea,0xf8,0x3c,0x81,0x4b,0x94,0xf7 };
        return ok && std::memcmp(digest, expected, sizeof(expected)) == 0;
    };
    binary_allowed = allowed();
    if (!binary_allowed)
        reshade::log::message(reshade::log::level::info,
            "[SLBridge] [DepthPin] unknown ReShade build; stock Generic Depth behaviour (requires SHA-256 0CEE63F9C9F13F3AC909C5B4903F4DBB4B719A7AB3B4F13B0DEAF83C814B94F7 / 6.8.0.2155 x64)");
}

struct State
{
    bool eligible = false, enabled = true, config_valid = true;
    bool owned = false, failed = false, attempted = false;
    uint32_t status = 3; // 0 off, 1 honoured, 2 failed/pending, 3 gate rejected; NOT readiness
    uint32_t logged = 0;
};
static std::map<effect_runtime *, State> states;
static std::mutex state_mutex;
static void once(State &s, uint32_t bit, const char *message)
{
    if (!(s.logged & bit))
    {
        s.logged |= bit;
        reshade::log::message(reshade::log::level::info, message);
    }
}

// Strict integer parsing: missing is the default, malformed is unsupported.
// nullptr deliberately reads GD's GLOBAL config, not a preset/runtime file.
static bool setting(const char *section, const char *key, uint32_t fallback, uint32_t &value)
{
    value = fallback;
    size_t size = 0;
    if (!reshade::get_config_value(nullptr, section, key, nullptr, &size)) return true;
    if (!size || size > 32) return false;
    char text[32] = {};
    size_t capacity = sizeof(text);
    if (!reshade::get_config_value(nullptr, section, key, text, &capacity)) return false;
    const size_t length = strnlen_s(text, sizeof(text));
    // API 18 returns a double-NUL-terminated list; reject multiple values too.
    if (!length || length == sizeof(text) || capacity != length + 1) return false;
    const auto parsed = std::from_chars(text, text + length, value);
    return parsed.ec == std::errc{} && parsed.ptr == text + length;
}

// Pointer is callback-local. Never keep it in State, even through an effect reload.
static unsigned char *data(effect_runtime *runtime)
{
    uint64_t address = 0;
    runtime->get_private_data(reinterpret_cast<const uint8_t *>(&runtime_guid), &address);
    if (!address || address % alignof(uint64_t)) return nullptr;
    return reinterpret_cast<unsigned char *>(static_cast<uintptr_t>(address));
}
static Prefix read(const unsigned char *p)
{
    Prefix result;
    std::memcpy(&result, p, sizeof(result));
    return result;
}
static void write(unsigned char *p, uint64_t value)
{
    // Aligned eight-byte field; memcpy avoids aliasing another module's C++ type.
    std::memcpy(p + offsetof(Prefix, override_depth), &value, sizeof(value));
}
static void release(State &s, unsigned char *p)
{
    if (s.owned && read(p).override_depth == window_depth) write(p, 0);
    s.owned = s.attempted = false;
}
static void read_enabled(State &s)
{
    uint32_t value = 1;
    const bool valid = setting("SL_BRIDGE", "PinDepth", 1, value);
    // Explicit retry: observe PinDepth=0 on reload, then nonzero on a later reload.
    if (valid && value != 0 && s.config_valid && !s.enabled) s.failed = false;
    s.config_valid = valid;
    s.enabled = value != 0;
}
static void pin(effect_runtime *runtime, State &s)
{
    if (!s.eligible) return;
    bool suspended = false;
    if (!s.config_valid || !s.enabled)
    {
        suspended = true;
        s.status = s.config_valid ? 0 : 3;
        once(s, s.config_valid ? 2 : 4, s.config_valid ?
            "[SLBridge] [DepthPin] PinDepth=0; released owned override, GD auto (hands-off)" :
            "[SLBridge] [DepthPin] invalid PinDepth; released owned override, stock Generic Depth behaviour");
    }
    else
    {
        uint32_t copy = 0;
        // [DepthPin] Every enabled present reads ReShade's INI cache, plus GD
        // fields when eligible; the cost is not just an eight-byte store.
        // MUST re-read before EVERY present pin; GD's overlay changes this live.
        if (!setting("DEPTH", "DepthCopyBeforeClears", 0, copy) || copy != 0)
        {
            suspended = true;
            s.status = 3;
            once(s, 8, "[SLBridge] [DepthPin] DepthCopyBeforeClears nonzero/invalid; released owned override, pin suspended until 0");
        }
        else if (s.failed) { suspended = true; s.status = 2; }
    }
    // [DepthPin] Fully hands-off while suspended unless an owned pin needs release.
    if (suspended && !s.owned) { s.attempted = false; return; }
    unsigned char *const p = data(runtime);
    if (!p)
    {
        s.eligible = false;
        s.status = 3;
        once(s, 1, "[SLBridge] [DepthPin] GD runtime data unavailable; stock Generic Depth behaviour until runtime re-init");
        return;
    }
    if (suspended) { release(s, p); return; }
    if (read(p).override_depth != window_depth)
    {
        write(p, window_depth);
        s.owned = true; // Do not claim a pre-existing identical manual override.
    }
    s.attempted = true;
    s.status = 2; // Pending GD selection, not evidence of readiness.
    once(s, 16, "[SLBridge] [DepthPin] GD pin active (ReShade 6.8.0.2155, sha 0CEE63F9...) -> 0x008218000000821A");
}
static void init(effect_runtime *runtime)
{
    const std::lock_guard<std::mutex> lock(state_mutex);
    State &s = states[runtime];
    s = {};
    if (!binary_allowed) return;
    // GD registered first: eligibility is evaluated here AFTER its runtime init.
    if (runtime->get_device()->get_api() != device_api::opengl || !data(runtime))
    {
        once(s, 1, "[SLBridge] [DepthPin] unsupported runtime/API or GD runtime data absent; stock Generic Depth behaviour");
        return;
    }
    s.eligible = true;
    read_enabled(s);
    pin(runtime, s);
}
static void destroy(effect_runtime *runtime)
{
    const std::lock_guard<std::mutex> lock(state_mutex);
    states.erase(runtime); // GD may already have destroyed its data. No dereference.
}
static void reload(effect_runtime *runtime)
{
    const std::lock_guard<std::mutex> lock(state_mutex);
    const auto it = states.find(runtime);
    if (it != states.end() && it->second.eligible) read_enabled(it->second);
    // Defer override release/write to present on the owning render thread.
}
static void present(command_queue *, swapchain *swap, const rect *, const rect *, uint32_t, const rect *)
{
    if (!swap) return; // Eligibility already includes the cached binary gate.
    const std::lock_guard<std::mutex> lock(state_mutex);
    for (auto &[runtime, s] : states)
    {
        // API 18 has separate runtime/swapchain interfaces: never cast between them.
        if (s.eligible && runtime->get_device() == swap->get_device() &&
            runtime->get_hwnd() == swap->get_hwnd())
            pin(runtime, s);
    }
}
static void verify(effect_runtime *runtime)
{
    const std::lock_guard<std::mutex> lock(state_mutex);
    const auto it = states.find(runtime);
    if (it == states.end()) return;
    State &s = it->second;
    if (!s.eligible || !s.attempted || s.failed) return;
    unsigned char *const p = data(runtime);
    if (!p)
    {
        s.eligible = false;
        s.status = 3;
        once(s, 1, "[SLBridge] [DepthPin] GD runtime data unavailable; stock Generic Depth behaviour until runtime re-init");
        return;
    }
    const Prefix gd = read(p);
    // A stale nonzero SRV alone is insufficient: GD clears selected before its
    // allocation/view failure branches. Check BOTH after GD begin_effects.
    if (gd.override_depth != window_depth || gd.selected != window_depth || gd.srv == 0)
    {
        release(s, p);
        s.failed = true;
        s.status = 2;
        char message[384];
        std::snprintf(message, sizeof(message),
            "[SLBridge] [DepthPin] FAIL: DEPTH pin not honoured (GD selected 0x%016llX, SRV 0x%016llX); "
            "released owned override, GD auto next frame; latched off until runtime re-init or PinDepth 0/1 reload retry",
            static_cast<unsigned long long>(gd.selected), static_cast<unsigned long long>(gd.srv));
        once(s, 32, message);
    }
    else s.status = 1;
}
static void finish(effect_runtime *runtime, command_list *, resource_view, resource_view)
{
    verify(runtime);
}
static uint32_t status(effect_runtime *runtime)
{
    const std::lock_guard<std::mutex> lock(state_mutex);
    const auto it = states.find(runtime);
    return it == states.end() ? 3 : it->second.status;
}
} // namespace depth_pin

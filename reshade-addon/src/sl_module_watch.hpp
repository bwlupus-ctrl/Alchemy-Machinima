// [ActorKey] Loader notification callback does no allocation, logging or locking.
#pragma once
#if defined(_WIN32)
#include <Windows.h>
#include <psapi.h>
#include <algorithm>
#include <vector>
#include "sl_key_guard.hpp"
namespace SLKey {
class ModuleWatch {
    alignas(8) volatile LONG64 mState = 0;
    void* mCookie = nullptr;
    using Register = LONG (NTAPI*)(ULONG, void (CALLBACK*)(ULONG,const void*,void*),void*,void**);
    using Unregister = LONG (NTAPI*)(void*);
    static void CALLBACK changed(ULONG, const void*, void* context) {
        auto* self = static_cast<ModuleWatch*>(context);
        LONG64 old, next;
        do {
            old = InterlockedCompareExchange64(&self->mState, 0, 0);
            next = static_cast<LONG64>(nextModuleState(static_cast<uint64_t>(old), GetTickCount()));
        } while (InterlockedCompareExchange64(&self->mState, next, old) != old);
    }
public:
    bool notified() const { return mCookie != nullptr; }
    uint64_t read() { return static_cast<uint64_t>(InterlockedCompareExchange64(&mState,0,0)); }
    void start() {
        if (mCookie) return;
        auto reg = reinterpret_cast<Register>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification"));
        if (reg) reg(0, changed, this, &mCookie);
    }
    void stop() {
        if (!mCookie) return;
        auto unreg = reinterpret_cast<Unregister>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrUnregisterDllNotification"));
        if (unreg) unreg(mCookie);
        mCookie = nullptr;
    }
};
struct ModuleList {
    int verdict = 0;
    const char* step = "";
    std::vector<HMODULE> modules;
    void summary(ModuleList& out) const { out.verdict=verdict; out.step=step; }
    bool operator==(const ModuleList& b) const { return verdict == b.verdict && modules == b.modules; }
};
inline ModuleList enumerateModules() {
    ModuleList result;
    HMODULE handles[1024]; DWORD needed = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(),handles,sizeof(handles),&needed) || needed > sizeof(handles) || needed % sizeof(HMODULE)) {
        result.verdict = -1; result.step = "EnumProcessModules/truncation"; return result;
    }
    result.modules.assign(handles, handles+needed/sizeof(HMODULE));
    std::sort(result.modules.begin(), result.modules.end());
    return result;
}
inline ModuleList scanReShade() {
    ModuleList result = enumerateModules();
    if (result.verdict < 0) return result;
    for (HMODULE module : result.modules) {
        if (GetProcAddress(module,"ReShadeRegisterAddon") && GetProcAddress(module,"ReShadeUnregisterAddon")) result.verdict = 1;
    }
    return result;
}
} // namespace SLKey
#endif

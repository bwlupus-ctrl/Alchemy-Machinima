// [ActorKey] Included by sl_reshade_bridge.cpp after frame reader/g state.
#include "sl_key_bracket.hpp"
#include "sl_key_whitelist.hpp"
#if __has_include("sl_bridge_build_info.h")
#include "sl_bridge_build_info.h"
#else
#define SL_ADDON_SOURCE_SHA256 {0}
#define SL_ADDON_COMMIT ""
#define SL_ADDON_BUILD_STAMP "unconfigured - no key guard capability"
#endif
static SLKey::Brackets<effect_runtime> g_key_brackets;
static SLKey::ModuleWatch g_key_modules;
static HMODULE g_key_module=nullptr;
static SLReShade_ReportKeyGuard_t g_report_guard=nullptr;
static void key_fault()
{
    g_key_brackets.recover();
    SLKey::outsideBracket();
    if(!g.dead.exchange(true)) logf(reshade::log::level::error,"[SLBridge] actor-key bridge faulted; restored open brackets, session disabled");
}
static void key_report(const SLReShadeKeyGuard& report)
{
    SLKey::outsideBracket();
    if(g_report_guard) g_report_guard(&report);
}
static void key_skipped(uint64_t frame)
{
    SLReShadeKeyGuard r={sizeof(r),SLRESHADE_GUARD_SKIPPED,frame,0,0,0,0};
    key_report(r);
}
static void key_log_host()
{
    const HMODULE host=reshade::internal::get_reshade_module_handle();
    wchar_t path[32768]; DWORD ignored=0;
    DWORD n=GetModuleFileNameW(host,path,32768);
    DWORD size=n&&n<32768 ? GetFileVersionInfoSizeW(path,&ignored) : 0;
    std::vector<unsigned char> data(size);
    VS_FIXEDFILEINFO* version=nullptr; UINT bytes=0;
    const bool ok=size && GetFileVersionInfoW(path,0,size,data.data()) &&
        VerQueryValueW(data.data(),L"\\",reinterpret_cast<void**>(&version),&bytes) && bytes>=sizeof(*version);
    logf(reshade::log::level::info,"[SLBridge] ReShadeVersion export=%p file version=%u.%u.%u.%u exact binary gate=%s",
        GetProcAddress(host,"ReShadeVersion"),ok?HIWORD(version->dwFileVersionMS):0,ok?LOWORD(version->dwFileVersionMS):0,
        ok?HIWORD(version->dwFileVersionLS):0,ok?LOWORD(version->dwFileVersionLS):0,
        depth_pin::binary_allowed?"PASS":"REJECTED (expected 6.8.0.2155)");
}
static void key_handshake(effect_runtime* runtime)
{
    SLKey::outsideBracket();
    g_key_modules.start();
    static bool notify_warned=false;
    if(!g_key_modules.notified()&&!notify_warned) {
        notify_warned=true;
        logf(reshade::log::level::warning,"[SLBridge] DLL notification unavailable; scanning modules twice every ARM/KEY present");
    }
    HMODULE exe=GetModuleHandleW(nullptr);
    g_report_guard=reinterpret_cast<SLReShade_ReportKeyGuard_t>(GetProcAddress(exe,SLRESHADE_REPORT_GUARD_NAME));
    auto reader=reinterpret_cast<SLReShade_ReportReader_t>(GetProcAddress(exe,SLRESHADE_REPORT_READER_NAME));
    static const SLReShadeReaderInfo info={sizeof(info),SLRESHADE_ABI_VERSION,
#ifdef SL_EXPECTED_ADDON_SOURCE_SHA256
        SLRESHADE_READER_CAP_KEY_BYPASS|SLRESHADE_READER_CAP_KEY_GUARD,
#else
        0,
#endif
        0,SL_ADDON_BUILD_STAMP,SL_ADDON_COMMIT,SL_ADDON_SOURCE_SHA256};
    if(reader) reader(&info);
    else logf(reshade::log::level::warning,"[SLBridge] viewer has no ReportReader; stale check unavailable");
    char source[65]={}; const char* hex="0123456789abcdef";
    for(unsigned i=0;i<32;++i) { source[2*i]=hex[info.source_sha256[i]>>4]; source[2*i+1]=hex[info.source_sha256[i]&15]; }
    logf(reshade::log::level::info,"[SLBridge] build %s commit %s src %s abi 1.3 caps 0x%X",info.build_stamp,info.commit,source,info.caps);
    key_log_host();
    if(!g_key_brackets.init(runtime,runtime->get_device(),uintptr_t(runtime->get_hwnd())))
        logf(reshade::log::level::warning,"[SLBridge] actor-key: >8 runtimes, bracket unavailable (%p)",runtime);
    // Folder validation at init, and again on every ARM/KEY present.
    auto folder=SLKey::scanFolder(g_key_module);
    if(folder.verdict<0) logf(reshade::log::level::warning,"[SLBridge] actor-key add-on check failed: %s",folder.step);
}
static void key_init(effect_runtime* runtime)
{
    __try { key_handshake(runtime); }
    __except(EXCEPTION_EXECUTE_HANDLER) { key_fault(); }
}
static void key_destroy(effect_runtime* runtime)
{
    __try { g_key_brackets.destroy(runtime); }
    __except(EXCEPTION_EXECUTE_HANDLER) { key_fault(); }
}
static void key_do_reload(effect_runtime* runtime)
{
    g_key_brackets.reload(runtime,GetTickCount());
    SLReShadeFrame f{};
    if(read_viewer_frame(f) && !(f.flags&(SLRESHADE_FLAG_ACTOR_KEY|SLRESHADE_FLAG_ACTOR_KEY_ARM))) {
        SLReShadeKeyGuard r={sizeof(r),SLRESHADE_GUARD_UNSETTLED,f.frame_counter,0,SLRESHADE_GUI_RELOADED,0,0};
        key_report(r); // REFUSED has no bracket; reload must still be observable.
    }
}
static void key_reload(effect_runtime* runtime)
{
    __try { key_do_reload(runtime); }
    __except(EXCEPTION_EXECUTE_HANDLER) { key_fault(); }
}
static void key_count(effect_runtime* runtime,unsigned which)
{
    // Off-present threads touch only this registry and its lock. No bridge/depth calls.
    __try { g_key_brackets.count(runtime,which); }
    __except(EXCEPTION_EXECUTE_HANDLER) { key_fault(); }
}
static void key_technique(effect_runtime* runtime,effect_technique,command_list*,resource_view,resource_view) { key_count(runtime,0); }
static void key_begin(effect_runtime* runtime,command_list*,resource_view,resource_view) { key_count(runtime,1); }
static void key_finish(effect_runtime* runtime,command_list*,resource_view,resource_view) { key_count(runtime,2); }
static bool key_overlay(effect_runtime* runtime,bool open,reshade::api::input_source)
{
    __try { g_key_brackets.overlay(runtime,open); }
    __except(EXCEPTION_EXECUTE_HANDLER) { key_fault(); }
    return false; // Never veto ReShade's UI.
}
static uint32_t key_gui_flags(effect_runtime* runtime)
{
    uint32_t flags=0,value=0;
    const char* keys[]={"ShowFPS","ShowClock","ShowFrameTime","ShowPresetName"};
    for(const char* key:keys) if(reshade::get_config_value(runtime,"OVERLAY",key,value)&&value==1) flags|=SLRESHADE_GUI_OSD;
    if(reshade::get_config_value(runtime,"OVERLAY","TutorialProgress",value)&&value==0) flags|=SLRESHADE_GUI_TUTORIAL;
    return flags;
}
static void key_idle(swapchain* swap)
{
    bool stale=false;
    auto* rt=g_key_brackets.idle(swap->get_device(),uintptr_t(swap->get_hwnd()),stale);
    if(rt) logf(reshade::log::level::info,"[SLBridge] actor-key bracket last close (runtime %p)",rt);
    if(stale) logf(reshade::log::level::error,"[SLBridge] actor-key stale open bracket restored on unkeyed present; bridge dead");
}
static void key_raw_present(swapchain* swap,uint64_t& requested)
{
    SLKey::outsideBracket();
    SLReShadeFrame f{};
    if(!read_viewer_frame(f)) {
        static uint64_t key_frames_missed=0; ++key_frames_missed;
        key_idle(swap); return; // Missing/torn = no bracket; heartbeat times out.
    }
    if(SLReShade_ABI_Minor(f.abi_version)<3) {
        static bool logged=false;
        if(!logged) { logged=true; logf(reshade::log::level::warning,"[SLBridge] viewer predates actor-key flag; bypass unavailable"); }
        return;
    }
    if(!(f.flags&(SLRESHADE_FLAG_ACTOR_KEY|SLRESHADE_FLAG_ACTOR_KEY_ARM))) { key_idle(swap); return; }
    requested=f.frame_counter;
    if(g.dead||g_key_brackets.isDead()) { key_skipped(requested); return; }
    uint32_t verdict=SLRESHADE_GUARD_CLEAN,foreign=0,gui=0;
    if(!depth_pin::binary_allowed) { verdict=SLRESHADE_GUARD_FOREIGN_ADDON; gui|=SLRESHADE_GUI_UNVERIFIED_HOST; }
    auto folder=SLKey::scanFolder(g_key_module);
    static SLKey::SettledScan<SLKey::WhitelistResult> modules;
    SLKey::WhitelistResult result;
    int check=modules.run(g_key_modules.notified(),GetTickCount(),[](){return g_key_modules.read();},
        [](){return SLKey::scanWhitelist(g_key_module,reshade::internal::get_reshade_module_handle());},result);
    foreign=SLKey::satAdd(folder.foreign,result.foreign);
    if(folder.verdict<0||check<0||foreign) {
        verdict=SLRESHADE_GUARD_FOREIGN_ADDON; gui|=folder.gui|result.gui;
        static const char* lastStep=nullptr;
        const char* step=folder.verdict<0?folder.step:result.step;
        if((folder.verdict<0||check<0)&&lastStep!=step) {
            lastStep=step; logf(reshade::log::level::warning,"[SLBridge] actor-key add-on check failed: %s",step);
        }
    } else if(check==2&&verdict==SLRESHADE_GUARD_CLEAN) verdict=SLRESHADE_GUARD_UNSETTLED;
    effect_runtime* selected=nullptr; bool first=false;
    if(!g_key_brackets.open(swap->get_device(),uintptr_t(swap->get_hwnd()),requested,verdict,foreign,gui,&selected,&first)) {
        static bool logged=false;
        if(!logged) { logged=true; logf(reshade::log::level::warning,"[SLBridge] actor-key: no unique live runtime / stale bracket, skipping"); }
        key_skipped(requested);
    } else if(first) logf(reshade::log::level::info,"[SLBridge] actor-key bracket first open (runtime %p)",selected);
}
static void key_present(command_queue*,swapchain* swap,const rect*,const rect*,uint32_t,const rect*)
{
    uint64_t requested=0;
    __try { key_raw_present(swap,requested); }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        key_fault();
        if(requested) { __try { key_skipped(requested); } __except(EXCEPTION_EXECUTE_HANDLER) {} }
    }
}
static void key_after_present(effect_runtime* runtime)
{
    SLReShadeKeyGuard report{}; bool changed=false;
    __try {
        if(g_key_brackets.finish(runtime,GetTickCount(),report,&changed)) {
            report.gui_flags|=key_gui_flags(runtime);
            if(changed && report.verdict==SLRESHADE_GUARD_CONTAMINATED)
                logf(reshade::log::level::warning,"[SLBridge] actor-key guard CONTAMINATED tech=%u calls=%u (runtime %p)",report.passes,report.effect_calls,runtime);
            key_report(report);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { key_fault(); }
}
// Wrappers prove the depth pin's mutex is never nested inside the bracket lock.
static void depth_present(command_queue* q,swapchain* s,const rect* a,const rect* b,uint32_t n,const rect* d)
{ SLKey::outsideBracket(); depth_pin::present(q,s,a,b,n,d); }
static void depth_finish(effect_runtime* r,command_list* c,resource_view a,resource_view b)
{ SLKey::outsideBracket(); depth_pin::finish(r,c,a,b); }

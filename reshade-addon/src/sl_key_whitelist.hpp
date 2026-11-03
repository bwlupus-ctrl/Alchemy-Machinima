// [ActorKey] Strict whitelist; only our add-on and the pinned host's built-ins.
#pragma once
#include "sl_module_watch.hpp"
#include <filesystem>
#include <set>
#include <string>
namespace SLKey {
inline bool addonExtension(std::wstring extension) {
    std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){return wchar_t(towlower(c));});
    return extension.rfind(L".addon",0)==0;
}
inline bool foreignModule(bool oursOrHost, const std::wstring& extension, bool init, bool uninit, bool name, bool description) {
    return !oursOrHost && (addonExtension(extension)||init||uninit||name||description);
}
struct WhitelistResult {
    int verdict=0; // 0 clean, -1 failure; 2 is reserved for unsettled
    uint32_t foreign=0, gui=0;
    const char* step="";
    std::vector<HMODULE> modules;
    std::vector<uint32_t> classification;
    void summary(WhitelistResult& out) const { out.verdict=verdict; out.foreign=foreign; out.gui=gui; out.step=step; }
    bool operator==(const WhitelistResult& b) const {
        return verdict==b.verdict && foreign==b.foreign && gui==b.gui && modules==b.modules && classification==b.classification;
    }
};
inline WhitelistResult failed(const char* step) {
    WhitelistResult r; r.verdict=-1; r.gui=SLRESHADE_GUI_CHECK_FAILED; r.step=step; return r;
}
inline void logForeign(const std::wstring& path) {
    outsideBracket();
    static std::set<std::wstring> logged;
    if(logged.insert(path).second) {
        char utf8[32768];
        if(WideCharToMultiByte(CP_UTF8,0,path.c_str(),-1,utf8,sizeof(utf8),nullptr,nullptr))
            reshade::log::message(reshade::log::level::warning,(std::string("[SLBridge] actor-key foreign add-on: ")+utf8).c_str());
    }
}
inline WhitelistResult scanWhitelist(HMODULE ours, HMODULE host) {
    WhitelistResult r;
    ModuleList list=enumerateModules();
    if(list.verdict<0) return failed(list.step);
    r.modules=list.modules;
    for(HMODULE m:list.modules) {
        wchar_t path[32768]; DWORD n=GetModuleFileNameW(m,path,32768);
        if(!n||n>=32768) return failed("GetModuleFileName/truncation");
        bool foreign=foreignModule(m==ours||m==host,std::filesystem::path(path).extension().wstring(),
            GetProcAddress(m,"AddonInit")!=nullptr,GetProcAddress(m,"AddonUninit")!=nullptr,
            GetProcAddress(m,"NAME")!=nullptr,GetProcAddress(m,"DESCRIPTION")!=nullptr);
        r.classification.push_back(foreign);
        if(foreign) { ++r.foreign; logForeign(path); }
    }
    return r;
}
inline WhitelistResult scanFolder(HMODULE ours) {
    char base[32768]={},value[32768]={}; size_t size=sizeof(base);
    reshade::get_reshade_base_path(base,&size);
    if(!size||size>=sizeof(base)||!memchr(base,0,sizeof(base))) return failed("base path/truncation");
    size=0;
    bool present=reshade::get_config_value(nullptr,"ADDON","AddonPath",nullptr,&size);
    if(present) {
        if(!size||size>sizeof(value)) return failed("AddonPath/truncation");
        size=sizeof(value);
        if(!reshade::get_config_value(nullptr,"ADDON","AddonPath",value,&size)||size>sizeof(value)||
            !memchr(value,0,sizeof(value))) return failed("AddonPath/truncation");
    }
    auto wide=[](const char* utf8) {
        wchar_t buffer[32768]; int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8,-1,buffer,32768);
        return n ? std::wstring(buffer) : std::wstring();
    };
    std::wstring basew=wide(base), valuew=present ? wide(value) : L"";
    if(basew.empty()||(present&&value[0]&&valuew.empty())) return failed("path encoding");
    std::filesystem::path directory=std::filesystem::path(basew)/valuew;
    const std::wstring dir=directory.wstring();
    if(dir.empty()||dir.size()>32750||dir.find_first_of(L"<>\"|?*")!=std::wstring::npos||
        std::any_of(dir.begin(),dir.end(),[](wchar_t c){return c<32;})) return failed("unformable path");
    wchar_t own[32768]; DWORD n=GetModuleFileNameW(ours,own,32768);
    if(!n||n>=32768) return failed("own add-on path");
    WIN32_FIND_DATAW data{};
    HANDLE find=FindFirstFileW((directory/L"*").c_str(),&data);
    WhitelistResult r;
    if(find==INVALID_HANDLE_VALUE) {
        DWORD err=GetLastError();
        return err==ERROR_PATH_NOT_FOUND||err==ERROR_FILE_NOT_FOUND ? r : failed("FindFirstFile");
    }
    do {
        if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && addonExtension(std::filesystem::path(data.cFileName).extension().wstring())) {
            // Compare full canonical paths: a same-named copy in another folder is foreign.
            wchar_t candidate[32768], ownFull[32768];
            DWORD a=GetFullPathNameW((directory/data.cFileName).c_str(),32768,candidate,nullptr);
            DWORD b=GetFullPathNameW(own,32768,ownFull,nullptr);
            if(!a||a>=32768||!b||b>=32768) { FindClose(find); return failed("full add-on path"); }
            if(_wcsicmp(candidate,ownFull)) { ++r.foreign; logForeign(candidate); }
        }
    } while(FindNextFileW(find,&data));
    DWORD err=GetLastError(); FindClose(find);
    if(err!=ERROR_NO_MORE_FILES) return failed("FindNextFile");
    return r;
}
} // namespace SLKey

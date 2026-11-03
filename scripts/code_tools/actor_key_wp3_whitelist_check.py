"""Injected Win32 / ReShade seams execute the production whitelist implementation."""
from pathlib import Path
import subprocess
from native_check_tools import compile_run,check_directory
ROOT=Path(__file__).resolve().parents[2]

def run():
    source=r'''
#include <Windows.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
#include <iostream>
#include "sl_key_bracket.hpp"
struct Fake {
 bool enumFail=false,truncate=false,pathFail=false,pathFull=false,baseFull=false,configFull=false,present=false;
 unsigned exports=0;std::wstring extension=L".dll";std::string config;
 DWORD firstError=0,nextError=ERROR_NO_MORE_FILES;
 std::vector<std::wstring> files;size_t at=0;
} f;
BOOL fakeEnum(HANDLE,HMODULE* list,DWORD size,DWORD* needed){
 *needed=f.truncate?size+sizeof(HMODULE):3*sizeof(HMODULE);
 if(f.enumFail)return FALSE;
 list[0]=HMODULE(1);list[1]=HMODULE(2);list[2]=HMODULE(3);return TRUE;
}
DWORD fakeFilename(HMODULE m,wchar_t* p,DWORD size){
 if(f.pathFail)return 0;if(f.pathFull)return size;
 auto text=m==HMODULE(2)?std::wstring(L"C:\\addons\\ours.addon"):std::wstring(L"C:\\viewer\\ordinary")+f.extension;
 wcscpy_s(p,size,text.c_str());return DWORD(text.size());
}
FARPROC fakeProc(HMODULE m,const char* name){
 if(m!=HMODULE(3))return nullptr;
 const char* names[]={"AddonInit","AddonUninit","NAME","DESCRIPTION"};
 for(unsigned i=0;i<4;++i)if((f.exports&(1<<i))&&!strcmp(name,names[i]))return FARPROC(1);
 return nullptr;
}
HANDLE fakeFirst(const wchar_t*,WIN32_FIND_DATAW* data){
 if(f.firstError){SetLastError(f.firstError);return INVALID_HANDLE_VALUE;}
 if(f.files.empty()){SetLastError(ERROR_FILE_NOT_FOUND);return INVALID_HANDLE_VALUE;}
 f.at=0;data->dwFileAttributes=0;wcscpy_s(data->cFileName,f.files[0].c_str());return HANDLE(10);
}
BOOL fakeNext(HANDLE,WIN32_FIND_DATAW* data){
 if(++f.at==f.files.size()){SetLastError(f.nextError);return FALSE;}
 wcscpy_s(data->cFileName,f.files[f.at].c_str());return TRUE;
}
BOOL fakeClose(HANDLE){return TRUE;}
namespace reshade {
 namespace log {enum class level {warning};inline void message(level,const char*){}}
 inline void get_reshade_base_path(char* p,size_t* n){if(f.baseFull){*n=32768;return;}strcpy_s(p,*n,"C:\\addons");*n=10;}
 inline bool get_config_value(void*,const char*,const char*,char* p,size_t* n){
  if(!f.present)return false;
  if(f.configFull){*n=32769;return true;}
  if(!p){*n=f.config.size()+1;return true;}strcpy_s(p,*n,f.config.c_str());*n=f.config.size()+1;return true;
 }
}
#define K32EnumProcessModules fakeEnum
#define GetModuleFileNameW fakeFilename
#define GetProcAddress fakeProc
#define FindFirstFileW fakeFirst
#define FindNextFileW fakeNext
#define FindClose fakeClose
#include "sl_key_whitelist.hpp"
using namespace SLKey;
int main(){
 assert(scanWhitelist(HMODULE(2),HMODULE(1)).foreign==0);
 f.extension=L".addon64";assert(scanWhitelist(HMODULE(2),HMODULE(1)).foreign==1);f={};
 for(unsigned i=0;i<4;++i){f.exports=1<<i;assert(scanWhitelist(HMODULE(2),HMODULE(1)).foreign==1);}f={};
 f.enumFail=true;assert(scanWhitelist(HMODULE(2),HMODULE(1)).verdict<0);f={};
 f.truncate=true;assert(scanWhitelist(HMODULE(2),HMODULE(1)).verdict<0);f={};
 f.pathFail=true;assert(scanWhitelist(HMODULE(2),HMODULE(1)).verdict<0);f={};
 f.pathFull=true;assert(scanWhitelist(HMODULE(2),HMODULE(1)).verdict<0);f={};
 // Missing AddonPath uses base. An empty or absent folder is clean.
 assert(scanFolder(HMODULE(2)).verdict==0);f.files={L"ours.addon"};assert(scanFolder(HMODULE(2)).foreign==0);
 f.files.push_back(L"disabled.addon64");assert(scanFolder(HMODULE(2)).foreign==1);f={};
 for(DWORD error:{ERROR_PATH_NOT_FOUND,ERROR_FILE_NOT_FOUND}){f.firstError=error;assert(scanFolder(HMODULE(2)).verdict==0);}f={};
 f.firstError=ERROR_ACCESS_DENIED;assert(scanFolder(HMODULE(2)).verdict<0);f={};
 f.files={L"ours.addon"};f.nextError=ERROR_ACCESS_DENIED;assert(scanFolder(HMODULE(2)).verdict<0);f={};
 f.baseFull=true;assert(scanFolder(HMODULE(2)).verdict<0);f={};
 f.present=true;f.configFull=true;assert(scanFolder(HMODULE(2)).verdict<0);f={};
 f.present=true;f.config="bad*path";assert(scanFolder(HMODULE(2)).verdict<0);f={};
 f.present=true;f.config="C:\\other";f.files={L"ours.addon"};assert(scanFolder(HMODULE(2)).foreign==1);f={};
 assert(foreignModule(false,L".ADDON64",false,false,false,false));
 assert(!foreignModule(true,L".addon64",true,true,true,true));
 std::cout<<"U-WL PASS production module/folder scan, each export, disabled/foreign file, same-name copy, missing key/directory, all enumeration/truncation/path failures\n";
}
'''
    result=compile_run(check_directory('actor_key_wp3'),source,'whitelist',flags=['/O2','/DNOMINMAX',f'/I"{ROOT / "reshade-addon/src"}"'])
    print(result.stdout,end='');assert result.returncode==0,result.stderr
if __name__=='__main__':run()

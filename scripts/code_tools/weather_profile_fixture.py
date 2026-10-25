"""Opt-in CPU + asynchronous GL_TIMESTAMP instrumentation for native fixtures.

No query result read occurs in a production path. Results are read after the
benchmark drains, and profiling is disabled for the acceptance measurements.
"""
import ctypes as C


def extend(prefix, methods, suffix, entries, sources):
    names=[('glGenQueries','void','int,unsigned*',None,[C.c_int,C.c_void_p]),
           ('glQueryCounter','void','unsigned,unsigned',None,[C.c_uint,C.c_uint]),
           ('glGetQueryObjectui64v','void','unsigned,unsigned,unsigned long long*',None,[C.c_uint,C.c_uint,C.c_void_p])]
    declarations='\n'.join(f'{ret} (*{name})({args});' for name,ret,args,_,_ in names)+'\n'
    source=declarations+PROFILE
    prefix=prefix.replace('int sceneKind=0;',source+'\nint sceneKind=0;')
    prefix+='\n#undef LL_PROFILE_GPU_ZONE\n#define LL_PROFILE_GPU_ZONE(x) WEATHER_FIXTURE_PASS(x)\n'
    inject='\n'.join(f'{name}=reinterpret_cast<decltype({name})>(p[{len(entries)+i}]);' for i,(name,*_) in enumerate(names))
    suffix=suffix.replace('LLShaderMgr::instance()->root=root;',inject+'\nLLShaderMgr::instance()->root=root;')
    methods+='\n'+API
    return prefix,methods,suffix,entries+names,sources


PROFILE=r'''
#include <chrono>
struct WeatherProfileRecord {const char* name=nullptr;double cpu=0;};
unsigned profileQueries[32768]{};WeatherProfileRecord profileRecords[16384];
unsigned profileCount=0;bool profileActive=false,profileAllocated=false;
struct WeatherFixturePass {
 int index=-1;std::chrono::steady_clock::time_point start;
 WeatherFixturePass(const char* name){
  if(!profileActive || profileCount>=16384)return;
  index=int(profileCount++);profileRecords[index].name=name;
  glQueryCounter(profileQueries[index*2],0x8E28);start=std::chrono::steady_clock::now();
 }
 ~WeatherFixturePass(){if(index<0)return;
  profileRecords[index].cpu=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  glQueryCounter(profileQueries[index*2+1],0x8E28);
 }
};
#define WEATHER_PROFILE_JOIN2(a,b) a##b
#define WEATHER_PROFILE_JOIN(a,b) WEATHER_PROFILE_JOIN2(a,b)
#define WEATHER_FIXTURE_PASS(x) WeatherFixturePass WEATHER_PROFILE_JOIN(weatherPass,__COUNTER__)(x)
'''
API=r'''
extern "C" __declspec(dllexport) void profile_start(){
 if(!profileAllocated){glGenQueries(32768,profileQueries);profileAllocated=true;}profileCount=0;profileActive=true;
}
extern "C" __declspec(dllexport) void profile_stop(){profileActive=false;}
extern "C" __declspec(dllexport) unsigned profile_count(){return profileCount;}
extern "C" __declspec(dllexport) const char* profile_name(unsigned i){return profileRecords[i].name;}
extern "C" __declspec(dllexport) void profile_values(double* out){
 for(unsigned i=0;i<profileCount;++i){unsigned long long a,b;
  glGetQueryObjectui64v(profileQueries[i*2],0x8866,&a);glGetQueryObjectui64v(profileQueries[i*2+1],0x8866,&b);
  out[i*2]=profileRecords[i].cpu;out[i*2+1]=double(b-a)/1e6;
 }
}
'''

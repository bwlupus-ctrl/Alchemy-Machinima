"""WP3 production guard/bracket/whitelist fixtures (no viewer/add-on build)."""
from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import sys
from native_check_tools import compile_run, check_directory, vcvars64

ROOT=Path(__file__).resolve().parents[2]
OUT=check_directory('actor_key_wp3')
FLAGS=['/O2','/DNOMINMAX','/DSL_ADDON_TESTS',f'/I"{ROOT / "reshade-addon/src"}"']

def native():
    source=r'''
#include "sl_key_bracket.hpp"
#include "sl_module_watch.hpp"
#include <atomic>
#include <thread>
#include <iostream>
#include <vector>
#include <cstddef>
using namespace SLKey;
struct Runtime {
 bool enabled=true, fault=false; unsigned writes=0;
 bool get_effects_state(){return enabled;}
 void set_effects_state(bool v){enabled=v;++writes;if(fault){fault=false;RaiseException(0xE1234567,0,0,nullptr);}}
};
bool faultOpen(Brackets<Runtime>& r,Runtime& rt){
 __try {return r.open(&rt,1,1,1,0,0);}
 __except(EXCEPTION_EXECUTE_HANDLER){r.recover();return false;}
}
SLReShadeKeyGuard clean(uint64_t f,uint32_t verdict=1){return {32,verdict,f,0,0,0,0};}
void abi(){
 static_assert(sizeof(SLReShadeFrame)==440 && offsetof(SLReShadeFrame,reserved)==344);
 static_assert(sizeof(SLReShadeFrameV11Tail)==96);
 static_assert(offsetof(SLReShadeReaderInfo,source_sha256)==96);
 SLReShadeReaderInfo a{},b{};a.struct_size=8;assert(!SLReShade_CopyReader(&b,&a));
 for(unsigned size:{16u,128u,256u}){memset(&a,'x',sizeof(a));a.struct_size=size;
  assert(SLReShade_CopyReader(&b,&a));assert(b.build_stamp[63]==0&&b.commit[15]==0);
  if(size==16)assert(b.source_sha256[0]==0);
 }
 SLReShadeKeyGuard g{},h{};g.struct_size=31;assert(!SLReShade_CopyGuard(&h,&g));
 g.struct_size=32;assert(SLReShade_CopyGuard(&h,&g));g.struct_size=256;assert(SLReShade_CopyGuard(&h,&g));
 SLReShadeReaderInfo reader{};reader.struct_size=128;reader.abi_version=0x10003;reader.caps=3;
 assert(readerCapable(0x10003,reader));assert(!readerCapable(0x10002,reader));reader.caps=1;assert(!readerCapable(0x10003,reader));reader.caps=3;
 const char* zeros="0000000000000000000000000000000000000000000000000000000000000000";
 assert(sourceMatches(reader,zeros));reader.source_sha256[31]=1;assert(!sourceMatches(reader,zeros));assert(!sourceMatches(reader,""));
 std::cout<<"U-ABI PASS frozen frame/tail/report offsets, bounded 8/16/128/256 copies\n";
}
void guard(){
 Guard g;SLReShadeKeyGuard none{};
 assert(g.evaluate(1,0,false,false,false,0,none));assert(g.state==State::NoReShade);
 for(int missing=0;missing<3;++missing){g.reset();assert(!g.evaluate(1,1,missing>0,missing>1,false,0,none));assert(g.state==State::Refused);}
 g.reset();assert(!g.evaluate(10,1,true,true,true,0,clean(9)));assert(g.arm&&g.epoch==10);
 for(unsigned i=0;i<60;++i){assert(!g.evaluate(10+i,1,true,true,true,0,none));assert(g.arm);g.publish(10+i,0,false);}
 Guard last=g;
 assert(!g.evaluate(70,1,true,true,true,0,none));assert(!g.arm&&g.state==State::Refused);
 assert(last.evaluate(70,1,true,true,true,0,clean(69))); // report in 60th swap wins
 for(unsigned verdict:{2u,3u,4u}){g=last;g.publish(70,0,true);assert(!g.evaluate(71,1,true,true,true,0,clean(70,verdict)));assert(g.state==State::Refused);}
 g=last;g.publish(70,0,true);assert(!g.evaluate(71,1,true,true,true,0,none));assert(g.state==State::Arming&&g.heartbeatMiss==1);
 g=last;g.publish(70,0,true);assert(!g.evaluate(71,1,true,true,true,1,clean(70)));assert(g.arm&&g.reason==Reason::Settling);
 g=last;g.publish(70,0,true);assert(!g.evaluate(71,1,true,true,true,0,clean(70,5)));assert(g.arm&&g.armPresents==0);
 g.refuse(Reason::Activity);assert(!g.evaluate(72,2,true,true,true,1,none));assert(g.state==State::Refused);auto reload=clean(71,5);reload.gui_flags=16;
 assert(!g.evaluate(72,1,true,true,true,0,reload));assert(g.state==State::Arming);
 // Exhaustive present/absent/stale/contaminated report sequences, length 1..5.
 for(int length=1;length<=5;++length){unsigned combos=1u<<(2*length);
  for(unsigned sequence=0;sequence<combos;++sequence){Guard model;SLReShadeKeyGuard r{};
   for(int i=0;i<length;++i){unsigned choice=(sequence>>(2*i))&3;
    if(model.havePublished){r=clean(model.lastPublished);if(choice==1)r={};if(choice==2)--r.viewer_frame;if(choice==3)r.verdict=2;}
    bool allowed=model.evaluate(100+i,1,true,true,true,0,r);
    if(allowed)assert(model.havePublished&&r.viewer_frame==model.lastPublished&&r.verdict==1);
    model.publish(100+i,0,allowed);
   }
  }
 }
 std::cout<<"U-GUARD PASS states, epoch, 60-present boundary, heartbeat exhaustive sequences, reload, source mismatch\n";
}
void brackets(){
 Brackets<Runtime> b;Runtime rt[9];
 for(int i=0;i<8;++i)assert(b.init(&rt[i],&rt[i],1));assert(!b.init(&rt[8],&rt[8],1));
 assert(!b.open(&rt[8],1,1,1,0,0));
 assert(b.open(&rt[0],1,1,1,0,0));assert(!rt[0].enabled);b.destroy(&rt[0]);assert(rt[0].enabled);
 assert(b.init(&rt[0],&rt[0],1));assert(b.open(&rt[0],1,1,1,0,0));assert(b.init(&rt[0],&rt[0],1));assert(rt[0].enabled);
 assert(b.open(&rt[0],1,1,1,0,0));b.reload(&rt[0],500);assert(!rt[0].enabled);
 rt[0].enabled=true;SLReShadeKeyGuard r{};assert(b.finish(&rt[0],501,r));outsideBracket();assert(rt[0].enabled&&(r.gui_flags&16));
 rt[0].enabled=false;assert(b.open(&rt[0],1,2,4,7,0));assert(b.finish(&rt[0],502,r));assert(!rt[0].enabled&&r.verdict==4&&r.foreign_addons==7);
 rt[0].enabled=true;assert(b.open(&rt[0],1,3,1,0,0));assert(!b.open(&rt[0],1,4,1,0,0));assert(b.isDead()&&rt[0].enabled);
 Brackets<Runtime> f;Runtime faulty;f.init(&faulty,&faulty,1);faulty.fault=true;assert(!faultOpen(f,faulty));assert(faulty.enabled&&f.isDead());outsideBracket();
 Brackets<Runtime> ambiguous;Runtime a,c;ambiguous.init(&a,&a,1);ambiguous.init(&c,&a,1);
 assert(!ambiguous.open(&a,1,1,1,0,0)&&a.writes==0&&c.writes==0);
 Brackets<Runtime> idle;Runtime idleRuntime;idle.init(&idleRuntime,&idleRuntime,1);
 assert(idle.open(&idleRuntime,1,7,1,0,0));bool stale=false;assert(idle.idle(&idleRuntime,1,stale)==&idleRuntime);
 assert(stale&&idleRuntime.enabled&&idle.isDead());outsideBracket();
 std::cout<<"U-BRACKET PASS capacity, unique match, destroy/reinit/stale, toggle, reload, FOREIGN brackets, SEH restoration\n";
 Brackets<Runtime> counts;Runtime one;counts.init(&one,&one,1);
 counts.count(&one,1);assert(counts.open(&one,1,1,1,0,0));counts.count(&one,2);assert(counts.finish(&one,0,r));assert(r.verdict==2&&r.passes==0&&r.effect_calls==2);
 counts.open(&one,1,2,1,0,0);counts.count(&one,0);counts.count(&one,1);counts.count(&one,2);counts.finish(&one,0,r);assert(r.passes==1&&r.effect_calls==2);
 counts.slots[0].begins[0]=UINT32_MAX;counts.count(&one,1);counts.slots[0].finishes[0]=UINT32_MAX;
 counts.open(&one,1,3,1,0,0);counts.finish(&one,0,r);assert(r.effect_calls==UINT32_MAX);
 assert(satAdd(UINT32_MAX,1)==UINT32_MAX);
 std::cout<<"U-COUNT PASS separate counters, off-present activity, saturation\n";
}
struct Result{int verdict=0;int identity=0;void summary(Result& b)const{b=*this;}bool operator==(const Result& b)const{return verdict==b.verdict&&identity==b.identity;}};
void gate(){
 SettledScan<Result> cache;uint64_t state=(uint64_t(1)<<32)|100;unsigned calls=0;Result out;
 auto read=[&](){return state;};auto scan=[&](){++calls;return Result{};};
 assert(cache.run(true,299,read,scan,out)==2&&calls==0&&!cache.valid);
 assert(cache.run(true,300,read,scan,out)==0&&calls==2&&cache.valid&&cache.settledState==state);
 assert(cache.run(true,799,read,scan,out)==0&&calls==2);
 assert(cache.run(true,800,read,scan,out)==0&&calls==4);
 state=(uint64_t(2)<<32)|801;assert(cache.run(true,801,read,scan,out)==2&&!cache.valid);
 assert(cache.run(true,1001,read,[&](){++calls;return Result{0,int(calls)};},out)==2&&!cache.valid);
 assert(cache.run(true,1001,read,[&](){++state;return Result{};},out)==2&&!cache.valid);
 state=0;assert(cache.run(false,0,read,scan,out)==0&&!cache.valid);unsigned n=calls;
 assert(cache.run(false,0,read,scan,out)==0&&calls==n+2);
 assert(cache.run(false,0,read,[](){return Result{-1,0};},out)==-1);
 state=(uint64_t(1)<<32)|uint32_t(UINT32_MAX-100);assert(cache.run(true,100,read,scan,out)==0); // wrap-safe quiet period
 // Interleave two callback updates at every CAS failure/retry point.
 for(unsigned first=0;first<2;++first) {
  uint64_t word=uint64_t(5)<<32, a=word,b=word;
  uint64_t desiredA=nextModuleState(a,100),desiredB=nextModuleState(b,101);
  if(first==0){word=desiredA;assert(word!=b);b=word;word=nextModuleState(b,102);}
  else {word=desiredB;assert(word!=a);a=word;word=nextModuleState(a,102);}
  assert(uint32_t(word>>32)==7&&uint32_t(word)==102);
 }
 assert(nextModuleState(uint64_t(UINT32_MAX)<<32,123)==123);
 // Real Win32 notification CAS callback under concurrent LoadLibrary activity.
 ModuleWatch watcher;watcher.start();auto before=watcher.read();
 HMODULE dll=LoadLibraryW(L"winhttp.dll");if(dll)FreeLibrary(dll);
 if(watcher.notified())assert(watcher.read()!=before);
 watcher.stop();
 std::cout<<"U-GATE PASS packed whole-word cache, quiet/two scans/race/no-notify/wrap/failure, real notification\n";
}
void concurrency(){
 Brackets<Runtime> b;Runtime r;b.init(&r,&r,1);std::atomic<bool> start=false;
 std::vector<std::thread> threads;
 for(unsigned t=0;t<4;++t)threads.emplace_back([&,t](){while(!start.load(std::memory_order_acquire))std::this_thread::yield();for(unsigned i=0;i<1000000;++i)b.count(&r,t%3);});
 start.store(true,std::memory_order_release);
 uint64_t dirty=0;for(unsigned i=0;i<1000000;++i){assert(b.open(&r,1,i,1,0,0));SLReShadeKeyGuard report{};assert(b.finish(&r,0,report));outsideBracket();
  if(report.passes||report.effect_calls){assert(report.verdict==2);++dirty;}else assert(report.verdict==1);
 }
 for(auto& t:threads)t.join();assert(b.totals[0]==2000000&&b.totals[1]==1000000&&b.totals[2]==1000000);
 assert(dirty>0&&r.enabled);
 std::cout<<"U-CONC PASS 4 worker threads / 1000000 presents / 4000000 exact event totals; contaminated="<<dirty<<"\nI-LOCKRULE PASS lock assertions silent; reports/depth calls outside lock\n";
}
int main(){abi();guard();brackets();gate();concurrency();}
'''
    exe=compile_run(OUT,source,'guard_bracket',flags=FLAGS,run=False)
    r=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=240)
    print(r.stdout,end='');assert r.returncode==0,r.stderr


def c_abi():
    source = '#include "llreshadebridgeabi.h"\n_Static_assert(sizeof(SLReShadeReaderInfo)==128,"reader");\n_Static_assert(sizeof(SLReShadeKeyGuard)==32,"guard");\nint main(void) { SLReShadeReaderInfo a={0},b={0}; a.struct_size=16; return !SLReShade_CopyReader(&b,&a); }\n'
    path=OUT/'abi_c.c';path.write_text(source)
    exe=OUT/'abi_c.exe';response=OUT/'abi_c.rsp';command=OUT/'abi_c.cmd'
    response.write_text('/nologo /TC /std:c11 /Fo"'+str(OUT/'abi_c.obj')+'" /Fe"'+str(exe)+'" /I"'+str(ROOT/'indra/newview')+'" "'+str(path)+'"')
    command.write_text('@echo off\ncall "'+str(vcvars64())+'" >nul\ncl @"'+str(response)+'"\nexit /b %ERRORLEVEL%\n')
    result=subprocess.run(['cmd','/d','/c',str(command)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    print('U-ABI C consumer PASS')


def hashes():
    spec=importlib.util.spec_from_file_location('sl_hash',ROOT/'cmake/SLAddonSourceHash.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    paths=[p.relative_to(ROOT).as_posix() for p in module.manifest(ROOT)]
    assert paths==sorted(paths,key=lambda s:s.encode())
    assert not any(p.endswith(('.fx','.fxh')) or '/external/' in p for p in paths)
    for destination in ('viewer','addon'):
        folder=OUT/destination;folder.mkdir(exist_ok=True)
        (folder/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.21)\nproject(hash_fixture NONE)\ninclude("'+(ROOT/'cmake/SLAddonSourceHash.cmake').as_posix()+'")\nsl_addon_source_hash("'+ROOT.as_posix()+'")\n')
        r=subprocess.run(['cmake','-S',str(folder),'-B',str(folder/'configured')],capture_output=True,text=True)
        assert r.returncode==0,r.stdout+r.stderr
    a=(OUT/'viewer/configured/sl_bridge_build_info.h').read_text().splitlines()[2]
    b=(OUT/'addon/configured/sl_bridge_build_info.h').read_text().splitlines()[2]
    assert a==b and module.digest(ROOT) in a
    fixture=OUT/'hash_tree';(fixture/'reshade-addon/src').mkdir(parents=True,exist_ok=True);(fixture/'indra/newview').mkdir(parents=True,exist_ok=True)
    for name in ('reshade-addon/src/a.HPP','reshade-addon/CMakeLists.txt','indra/newview/llreshadebridgeabi.h'):
        (fixture/name).write_bytes(b'line1\nline2\n')
    before=module.digest(fixture)
    for p in module.manifest(fixture):p.write_bytes(b'\xef\xbb\xbfline1\r\nline2\r')
    assert module.digest(fixture)==before
    (fixture/'reshade-addon/src/a.HPP').write_bytes(b'edited\n');assert module.digest(fixture)!=before
    print('I-HASH PASS independent viewer/add-on configure, sorted canonical manifest, BOM/CRLF/CR normalization, source edit detected')


def wiring():
    bridge=(ROOT/'indra/newview/llreshadebridge.cpp').read_text()
    actor=(ROOT/'indra/newview/alactorchromakey.cpp').read_text()
    assert 'if (gSnapshot) return true;' in actor and 'instance().keyGuardAllows(gFrameCount)' in actor
    assert 'else if (arm) f.flags |= SLRESHADE_FLAG_ACTOR_KEY_ARM' in bridge
    assert 'mGuardEvalFrame == gFrameCount && mKeyGuard.arm' in bridge
    assert 'mKeyGuard.publish(f.frame_counter, keyModuleWatch().read(), keyed)' in bridge
    assert 'ReShade overlay not hidden' in bridge and 'next.amber = loaded' in bridge
    addon=(ROOT/'reshade-addon/src/sl_reshade_bridge.cpp').read_text()
    assert 'sl_key_guard_events.inl' in addon
    assert 'SLKey::outsideBracket();' in addon
    print('WP3 wiring PASS K0/snapshot bypass, current-frame exclusive flags, capture-before-present, red/amber status API')


if __name__=='__main__':
    from actor_key_wp3_whitelist_check import run as whitelist
    hashes();native();c_abi();whitelist();wiring()

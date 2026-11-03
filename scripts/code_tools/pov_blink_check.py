"""POV blink native timing + production GLSL render fixture (hidden WGL, no viewer build).

Outputs Dark/Warm 4K PNGs and GPU-only timing under .tmp/blink_20261102.
Run in a VS developer environment, or supply --native-compiler and its environment.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import subprocess

import numpy as np
from PIL import Image
import relight_3d_gl as G

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.tmp/blink_20261102'
NATIVE = r'''
#include "alpovblinkmath.h"
#include <cassert>
#include <iostream>
using namespace ALPOVBlinkMath;
int main() {
    Params p;
    State s; s.blink(0., p);
    double previous = 0.;
    for (int i=0;i<=90;++i) {
        double value=s.sample(i*.001);
        assert(value+1e-10 >= previous); previous=value;
    }
    assert(std::abs(s.sample(.09)-1.)<1e-9);
    assert(s.sample(.11)==1. && s.sample(.129)==1.);
    previous=1.;
    for (int i=130;i<=310;++i) {
        double value=s.sample(i*.001);
        assert(value<=previous+1e-10); previous=value;
    }
    assert(s.sample(.31)==0. && s.sample(2.)==0.);
    p.depth=.5; s.blink(3.,p); assert(std::abs(s.sample(3.1)-.5)<1e-9);
    s.hold(4.,true,p); assert(s.sample(100.)==.5);
    s.hold(100.,false,p); assert(s.sample(100.)==.5);
    assert(s.sample(100.09)>.24 && s.sample(100.09)<.26);
    assert(s.sample(100.181)==0.);
    // Releasing during closure is continuous, including immediate/zero durations.
    s.hold(101.,true,p); double a=s.sample(101.04);
    s.hold(101.04,false,p); assert(std::abs(s.sample(101.04)-a)<1e-10);
    State instant; instant.blink(0.,{0.,0.,0.,1.}); assert(instant.sample(0.)==0.);
    for (double rate : {1.,15.,60.}) {
        Scheduler scheduler;
        for (int i=0;i<10000;++i) {
            double d=scheduler.delay(rate);
            assert(d>=Scheduler::refractory);
            assert(d>=std::max(.65,60./rate*.55));
            assert(d<=std::max(.65,60./rate*1.45));
        }
        State automatic; double now=0., last_end=-100.; bool active=false; int blinks=0;
        for (int i=0;i<120000;++i) {
            now=i*.01;
            double v=automatic.update(now,Params{},true,rate);
            if(v>0. && !active) { assert(now-last_end>=.65); active=true; ++blinks; }
            if(v==0. && active) { active=false; last_end=now; }
        }
        assert(blinks>0);
    }
    assert(!shouldDraw(0.,true) && !shouldDraw(.5,false));
    assert(shouldDraw(.5,true) && shouldDraw(1.,true));
    PadGesture pad;
    assert(!pad.sample(0.,0,64,64,true,true));
    assert(!pad.sample(.01,0,64,64,false,true));
    assert(!pad.sample(.02,64,64,64,false,true) && !pad.held);
    assert(pad.sample(.1,0,64,64,false,true) && !pad.held); // short click
    assert(!pad.sample(.2,64,64,64,false,true));
    assert(!pad.sample(.4,64,64,64,false,true) && pad.held);
    assert(!pad.sample(.5,0,64,64,false,true) && !pad.held); // release long hold
    assert(!pad.sample(.6,64,64,64,true,true) && !pad.held); // focus/device prime
    assert(!pad.sample(.9,64,64,64,false,true) && !pad.held);
    pad.sample(1.,0,64,64,false,true);
    pad.sample(1.01,64,64,64,false,true);
    pad.sample(1.21,64,64,64,false,true); assert(pad.held);
    assert(!pad.sample(1.3,64,64,64,false,false) && !pad.held);
    pad.sample(2.,0,32,16,true,true); pad.sample(2.01,0,32,16,false,true);
    assert(pad.sample(2.02,32,32,16,false,true));
    assert(!pad.sample(2.03,16,32,16,false,true) && pad.held);
    assert(!pad.sample(2.04,0,32,16,false,true) && !pad.held);
    std::cout << "PASS native gamepad: shared L3 tap/hold, separate buttons, prime/focus release\n";
    std::cout << "PASS native: asymmetric quintic timings/monotonicity, partial depth, hold/release continuity, zero durations, randomized bounds/refractory, idle/capture skip\n";
}
'''


def native(compiler):
    source = OUT / 'pov_blink_native.cpp'
    source.write_text(NATIVE, encoding='utf-8')
    exe = OUT / 'pov_blink_native.exe'
    command = [compiler, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX',
               '/I' + str(ROOT / 'indra/newview'), str(source),
               '/Fe:' + str(exe), '/Fo:' + str(OUT / 'pov_blink_native.obj')]
    subprocess.run(command, check=True, cwd=ROOT)
    subprocess.run([str(exe)], check=True, cwd=ROOT)
    # Use the existing fixture's parser doubles with the production migration
    # function. B represents the new camera key (the double accepts ASCII only).
    from test_key_binding_migration import build_source
    migration = build_source().split('int main() {', 1)[0] + r'''
int main() {
    using Mode=LLViewerInput::KeyMode;
    Mode defaults;
    defaults.bindings.add({"V","CTL_ALT","toggle_viewfinder"});
    defaults.bindings.add({"B","NONE","pov_eye_blink"});
    for(int version : {0,1,2,3}) {
        Mode user; migrateBindingMode(user,defaults,version,1);
        assert(user.bindings.size()==(version<2 ? 2u : version<3 ? 1u : 0u));
    }
    Mode occupied; occupied.bindings.add({"B","ALT","custom"});
    migrateBindingMode(occupied,defaults,2,1); assert(occupied.bindings.size()==1);
    Mode cleared; cleared.bindings.add({"","NONE","pov_eye_blink"});
    migrateBindingMode(cleared,defaults,2,1); assert(cleared.bindings.size()==1);
    Mode empty; migrateBindingMode(empty,defaults,0,0); assert(empty.bindings.empty());
    Mode user, additions; migrateBindingMode(user,defaults,2,1,&additions);
    migrateBindingMode(user,defaults,2,1); assert(user.bindings.size()==1 && additions.bindings.size()==1);
    assert(user.bindings[0].command.getValue()=="pov_eye_blink");
    std::cout << "PASS native keyboard migration: v2 adds blink, v3 preserves omissions, conflicts/custom/cleared bindings preserved, idempotent\n";
}
'''
    source.write_text(migration, encoding='utf-8')
    subprocess.run(command, check=True, cwd=ROOT)
    subprocess.run([str(exe)], check=True, cwd=ROOT)
    production_allocation(compiler)
    production_input(compiler)
    migration_roundtrip(compiler)
    admission(compiler)
    integration_contract()


def compile_controls(compiler, source, name, mutations):
    path=OUT/(name+'.cpp');exe=OUT/(name+'.exe')
    for mutation, pair in [*mutations.items(), (None, None)]:
        text=source
        if pair:
            old,new=pair;assert old in text
            text=text.replace(old,new)
        path.write_text(text,encoding='utf8')
        subprocess.run([compiler,'/nologo','/std:c++17','/EHsc','/W4','/WX',
            '/I'+str(ROOT/'indra/newview'),str(path),'/Fe:'+str(exe),
            '/Fo:'+str(OUT/(name+'.obj'))],check=True)
        result=subprocess.run([str(exe)])
        if mutation:
            assert result.returncode!=0,'Mutation survived: '+mutation
            print('PASS negative control:',mutation,'fails production-body assertions',flush=True)
        else:result.check_returncode()


def production_allocation(compiler):
    cpp=(ROOT/'indra/newview/alpovblink.cpp').read_text(encoding='utf8')
    a=cpp.index('    bool acquire(int width, int height)');b=cpp.index('    void copy(int y, int height)',a)
    method=cpp[a:b]
    source=r'''
#include <cassert>
#include <iostream>
using U32=unsigned;using GLint=int;using GLenum=unsigned;
constexpr U32 GL_DRAW_FRAMEBUFFER_BINDING=1,GL_DRAW_BUFFER=2,GL_DRAW_FRAMEBUFFER=3,GL_BACK_LEFT=4,
 GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE=5,GL_RGBA8=6,GL_RGBA=7,GL_RGB10_A2=8,GL_RGBA16F=9,GL_RGBA16=10;
int destination_bits=8, allocations=0;bool fail=false;U32 texture_names=0;
void glGetIntegerv(U32,GLint* v) {*v=0;}
void glGetFramebufferAttachmentParameteriv(U32,U32,U32,GLint* v) {*v=destination_bits;}
struct LLRenderTarget {
 U32 w=0,h=0,fmt=GL_RGBA8,id=++texture_names;bool depth=false,stencil=false;
 bool isComplete() const {return w>0 && h>0;}
 U32 getWidth() const {return w;} U32 getHeight() const {return h;}
 U32 getTexture() const {return w ? id : 0;}
 U32 getInternalFormat() const {return fmt;} U32 getDepth() const {return depth?1:0;}
 bool hasStencil() const {return stencil;}
 void release() {w=h=0;depth=false;}
 bool allocate(int width,int height,U32 format,bool use_depth=false) {
  ++allocations;assert(!use_depth);if(fail)return false;
  w=width;h=height;fmt=format;return true;
 }
};
LLRenderTarget sample_target, debug_scratch;
unsigned failed_width=0,failed_height=0;
struct Overlay {LLRenderTarget* scratch;U32 source=0;
'''+method+r'''
};
int main() {
 LLRenderTarget scratch;Overlay o{&scratch};
 assert(o.acquire(3840,2160) && allocations==1 && !sample_target.depth);
 assert(o.acquire(3840,2160) && allocations==1);
 assert(o.acquire(1920,1080) && allocations==2);
 sample_target.release();allocations=0;fail=true;
 assert(!o.acquire(3840,2160) && allocations==1);
 for(int i=0;i<100;++i) assert(!o.acquire(3840,2160));
 assert(allocations==1);assert(!o.acquire(1920,1080) && allocations==2);
 fail=false;sample_target.release();allocations=0;failed_width=failed_height=0;
 scratch.w=3840;scratch.h=2160;debug_scratch=scratch;
 assert(o.acquire(3840,2160));
 // Two-frame debug/capture oracle: the live blink writes its selected source;
 // subsequent sensor FXAA bypasses scratch refresh, then debug/capture reads it.
 U32 debug_content=1234, before=debug_content;
 if(o.source==debug_scratch.getTexture()) debug_content=9999;
 for(bool capture : {false,true}) {
  (void)capture;assert(debug_content==before);
 }
 assert(o.acquire(3840,2160) && allocations==1 && o.source==sample_target.getTexture());
 scratch.stencil=true;assert(o.acquire(3840,2160) && allocations==1 && o.source==sample_target.getTexture());
 assert(!sample_target.depth);scratch.stencil=false;
 destination_bits=10;scratch.fmt=GL_RGBA16;
 assert(o.acquire(3840,2160) && o.source==sample_target.getTexture() && sample_target.isComplete());
 // Precision at first allocation, as in production screen-buffer teardown.
 sample_target.release();
 scratch.w=0;
 assert(o.acquire(3840,2160) && sample_target.fmt==GL_RGB10_A2);
 std::cout<<"PASS production allocation policy: AA scratch untouched across live/sensor-debug/capture frames; colour-only fallback caches; failure retries 0 over 100 frames until resize; 10-bit precision\n";
}
'''
    compile_controls(compiler, source, 'pov_allocation', {
        'borrow_scratch': ('        if (failed_width',
            '        if (debug_scratch.isComplete()) { source=debug_scratch.getTexture(); return true; }\n        if (failed_width')})



def production_input(compiler):
    """Compile production conflict lookup and hold escape with minimal viewer doubles."""
    joystick=(ROOT/'indra/newview/llviewerjoystick.cpp').read_text(encoding='utf8')
    a=joystick.index('std::string LLViewerJoystick::povBlinkPadConflict(');b=joystick.index('\n}\n',a)+3
    conflict=joystick[a:b]
    header=(ROOT/'indra/newview/llviewerjoystick.h').read_text(encoding='utf8')
    a=header.index('typedef enum E_Buttons');b=header.index('} E_buttons;',a)+len('} E_buttons;')
    enums=header[a:b]
    blink=(ROOT/'indra/newview/alpovblink.cpp').read_text(encoding='utf8')
    a=blink.index('void ALPOVBlink::blink()');b=blink.index('void ALPOVBlink::renderLastSceneImage(',a)
    actions=blink[a:b]
    source=r'''
#include "alpovblinkmath.h"
#include <cassert>
#include <string>
#include <iostream>
using U32=unsigned;using S32=int;
bool dragon_enabled=false,hot_enabled=false;int gSavedSettings=0;
template<class T> struct LLCachedControl {
 bool hot;
 LLCachedControl(int&,const char* name,T):hot(std::string(name)=="DirectorHotGazeGamepad") {}
 T operator()() const {return static_cast<T>(hot?hot_enabled:dragon_enabled);}
 operator T() const {return operator()();}
};
'''+enums+r'''
struct LLViewerJoystick {
 bool mAvatarEnabled=true,mFlycamEnabled=true,mBuildEnabled=false;
 int mMappedButtons[MAX_BUTTONS];
 LLViewerJoystick() {for(int& b:mMappedButtons)b=-1;}
 std::string povBlinkPadConflict(U32) const;
};
'''+conflict+r'''
ALPOVBlinkMath::State state;
bool ui_held=false,pad_held=false,pad_escape=false;
double now=0.;
struct LLTimer {static double getTotalSeconds() {return now;}};
ALPOVBlinkMath::Params params() {return {};}
void updateHold() {state.hold(now,ui_held||pad_held,params());}
namespace ALPOVBlink {
void blink();void setUIHeld(bool);bool uiHeld();void setPadHeld(bool);
}
'''+actions+r'''
int main() {
 LLViewerJoystick j;
 for(U32 mask:{1U,2U,4U,8U}) {
  hot_enabled=false;assert(j.povBlinkPadConflict(mask).empty());
  hot_enabled=true;assert(j.povBlinkPadConflict(mask).find("Hot targets")!=std::string::npos);
 }
 hot_enabled=false;
 const U32 masks[]={16,32,64,128};const int indices[]={7,6,8,9};
 for(int k=0;k<4;++k) {
  assert(j.povBlinkPadConflict(masks[k]).empty());
  dragon_enabled=true;j.mMappedButtons[TOGGLE_RUN]=indices[k];
  assert(j.povBlinkPadConflict(masks[k]).find("Run toggle - pick another button")!=std::string::npos);
  j.mAvatarEnabled=false;assert(j.povBlinkPadConflict(masks[k]).empty());
  j.mAvatarEnabled=true;assert(!j.povBlinkPadConflict(masks[k]).empty());
  dragon_enabled=false;assert(j.povBlinkPadConflict(masks[k]).empty());
  j.mMappedButtons[TOGGLE_RUN]=-1;j.mMappedButtons[FLYCAM]=indices[k];
  assert(j.povBlinkPadConflict(masks[k]).find("Flycam toggle")!=std::string::npos);
  j.mFlycamEnabled=false;assert(j.povBlinkPadConflict(masks[k]).empty());
  j.mFlycamEnabled=true;assert(!j.povBlinkPadConflict(masks[k]).empty());
  j.mMappedButtons[FLYCAM]=-1;
  j.mMappedButtons[ZOOM_IN]=indices[k];
  j.mFlycamEnabled=false;assert(j.povBlinkPadConflict(masks[k]).empty());
  j.mFlycamEnabled=true;assert(!j.povBlinkPadConflict(masks[k]).empty());
  j.mMappedButtons[ZOOM_IN]=-1;
  // Every action, every module combination, and re-enable on the same object.
  for(int i=0;i<MAX_BUTTONS;++i) for(bool avatar : {false,true})
   for(bool flycam : {false,true}) for(bool build : {false,true})
    for(bool dragon : {false,true}) {
     j.mAvatarEnabled=avatar;j.mFlycamEnabled=flycam;j.mBuildEnabled=build;
     dragon_enabled=dragon;j.mMappedButtons[i]=indices[k];
     bool expected=(flycam && (i<=CROUCH || i==FLYCAM)) ||
      (avatar && dragon && (i==JUMP || i==CROUCH || i==FLY || i==MOUSELOOK || i==TOGGLE_RUN)) ||
      (build && dragon && (i==ROLL_LEFT || i==ROLL_RIGHT || i==JUMP || i==CROUCH));
     assert(j.povBlinkPadConflict(masks[k]).empty()==!expected);
     assert(j.mMappedButtons[i]==indices[k]); // predicate never rewrites mapping
     j.mMappedButtons[i]=-1;
    }
  j.mAvatarEnabled=j.mFlycamEnabled=true;j.mBuildEnabled=false;dragon_enabled=false;
 }
 // Personalized Xbox defaults: Run=Back, Flycam/Zoom reset=Start; L3 free.
 j.mMappedButtons[TOGGLE_RUN]=6;j.mMappedButtons[FLYCAM]=7;j.mMappedButtons[ZOOM_DEFAULT]=7;
 dragon_enabled=true;assert(j.povBlinkPadConflict(64).empty());
 ALPOVBlink::setUIHeld(true);now=1.;assert(state.sample(now)==1.);
 ALPOVBlink::blink();assert(!ALPOVBlink::uiHeld() && !state.held);
 now+=.181;assert(state.sample(now)==0.);
 ALPOVBlink::setUIHeld(true);now+=1.;assert(state.sample(now)==1.);
 ALPOVBlink::setPadHeld(true);assert(!ALPOVBlink::uiHeld() && !state.held);
 now+=.2;ALPOVBlink::setPadHeld(true);assert(!state.held && state.sample(now)==0.);
 ALPOVBlink::setPadHeld(false);ALPOVBlink::setPadHeld(true);assert(state.held);
 ALPOVBlink::setPadHeld(false);assert(!state.held);
 std::cout<<"PASS production input: all four pad choices, Run/BlackDragon and flycam conflicts, fork Xbox L3 free, F9 and pad UI-hold escape/rearm\n";
}
'''
    compile_controls(compiler, source, 'pov_input', {
        'disabled_modules': ('const bool active = (mFlycamEnabled && (i <= CROUCH || i == FLYCAM)) ||',
                            'const bool active = dragon() || i <= CROUCH || i == FLYCAM ||'),
        'hot_dpad_conflict': ('if (hot_gaze && (mask & 0x000fU))', 'if (false)')})
    for setting in ('JoystickAvatarEnabled','JoystickFlycamEnabled'):
        assert 'getControl("'+setting+'")' in joystick
        assert 'm'+('Avatar' if 'Avatar' in setting else 'Flycam')+'Enabled = gSavedSettings.getBOOL("'+setting+'")' in joystick
    dispatch=joystick[joystick.index('static LLCachedControl<U32> blink_button'):joystick.index('    mXIPrevRaw = raw;')]
    assert dispatch.count('povBlinkPadConflict(')==2 and '? (U16)blink_button() : 0' in dispatch


def migration_roundtrip(compiler):
    """Production loaders + migrations; parser/serialization doubles, four modes."""
    from test_key_binding_migration import build_source
    prefix=build_source().split('void migrateBindingMode(',1)[0]
    prefix=prefix.replace('#include <vector>','#include <vector>\n#include <fstream>\n#include <iomanip>')
    prefix=prefix.replace('*key = s.size() == 1 ? s[0] : KEY_NONE;','*key = s == "F9" ? 0x78 : s.size() == 1 ? s[0] : KEY_NONE;')
    prefix=prefix.replace('struct KeyMode { Bindings bindings; };',r'''
    struct KeyMode { Bindings bindings; bool provided=true; bool isProvided() const {return provided;} };
    struct Keys {
        KeyMode first_person, third_person, sitting, edit_avatar;
        bool validateBlock() const {return true;}
    };
    static void migrateKeysXML(const std::string&, Keys&, S32, Keys* = nullptr);
    static bool loadKeysXML(const std::string&, Keys&);
    S32 loadBindingMode(const KeyMode& mode,int) { return static_cast<S32>(mode.bindings.size()); }
''')
    prefix+=r'''
int marker=2;
struct Settings {int getS32(const char*) {return marker;}} gSavedSettings;
constexpr int LL_PATH_APP_SETTINGS=0,MODE_FIRST_PERSON=0,MODE_THIRD_PERSON=1,MODE_SITTING=2,MODE_EDIT_AVATAR=3;
struct Dir {std::string getExpandedFilename(int,const char*) {return "defaults";}} dir;
Dir* gDirUtilp=&dir;
std::vector<LLViewerInput::KeyMode*> modes(LLViewerInput::Keys& k) {
 return {&k.first_person,&k.third_person,&k.sitting,&k.edit_avatar};
}
void serialize(const std::string& file, LLViewerInput::Keys& keys) {
 std::ofstream out(file);
 for(auto* mode:modes(keys)) {
  out<<mode->bindings.size()<<'\n';
  for(auto& b:mode->bindings) out<<std::quoted(b.key)<<' '<<std::quoted(b.mask)<<' '<<std::quoted(b.command.value)<<'\n';
 }
}
struct LLSimpleXUIParser {
 bool readXUI(const std::string& file,LLViewerInput::Keys& keys) {
  if(file=="defaults") {
   for(auto* mode:modes(keys)) mode->bindings.add({"F9","NONE","pov_eye_blink"});
   return true;
  }
  std::ifstream in(file); if(!in) return false;
  for(auto* mode:modes(keys)) {
   int n=0; in>>n;
   for(int i=0;i<n;++i) {
    std::string key,mask,command;in>>std::quoted(key)>>std::quoted(mask)>>std::quoted(command);
    mode->bindings.push_back({key,mask,command.c_str()});
   }
  }
  return true;
 }
};
'''
    production=(ROOT/'indra/newview/llviewerinput.cpp').read_text(encoding='utf8')
    a=production.index('void migrateBindingMode(');b=production.index('\n}\n}',a)+2
    prefix+=production[a:b]+'\n'
    a=production.index('void LLViewerInput::migrateKeysXML(');b=production.index('S32 LLViewerInput::loadBindingsXML(',a)
    prefix+=production[a:b]
    prefix+=r'''
int main() {
 const std::string file=".tmp/blink_20261102/key_roundtrip.fixture";
 LLViewerInput::Keys original;
 for(auto* mode:modes(original)) mode->bindings.add({"W","NONE","push_forward"});
 serialize(file,original);
 // Preferences edits third-person only with Apply to all modes off.
 LLViewerInput::Keys edited;
 assert(LLViewerInput::loadKeysXML(file,edited));
 edited.third_person.bindings.add({"X","NONE","unrelated_edit"});
 serialize(file,edited); marker=3; // Same order as production Controls save.
 LLViewerInput::Keys reloaded;
 assert(LLViewerInput::loadKeysXML(file,reloaded));
 for(auto* mode:modes(reloaded)) {
  int count=0;
  for(auto& b:mode->bindings) if(b.command.value=="pov_eye_blink" && b.key=="F9") ++count;
  assert(count==1);
 }
 assert(reloaded.third_person.bindings.size()==3);
 std::cout<<"PASS production load/migration save/reload round trip: edit one mode, marker 3, F9 exactly once in all four modes (parser doubles)\n";
}
'''
    source=OUT/'pov_key_roundtrip.cpp';source.write_text(prefix,encoding='utf8');exe=OUT/'pov_key_roundtrip.exe'
    subprocess.run([compiler,'/nologo','/std:c++17','/EHsc','/W4','/WX',str(source),
                    '/Fe:'+str(exe),'/Fo:'+str(OUT/'pov_key_roundtrip.obj')],check=True)
    subprocess.run([str(exe)],check=True,cwd=ROOT)


# This is the same template invoked by renderLastSceneImage, with counting / GL
# backends. Mutating its idle return must fail executable assertions.
ADMISSION = r'''
#include "alpovblinkmath.h"
#include <cassert>
#include <vector>
#include <iostream>
using namespace ALPOVBlinkMath;
struct Counter {
 int allocations=0, binds=0, draws=0, copies=0, rows=0;
 bool acquire(int,int) { ++allocations; return true; }
 void copy(int y,int h) { assert(y>=0 && h>0 && y+h<=2160); ++copies; rows+=h; }
 void bind() { ++binds; }
 void drawSceneImage() { ++draws; }
};
#ifdef BRIDGE
using Callback = void(__cdecl*)(int,int,int);
struct GLBackend {
 Callback cb;
 bool acquire(int w,int h) { cb(0,w,h); return true; }
 void copy(int y,int h) { cb(3,y,h); }
 void bind() { cb(1,0,0); }
 void drawSceneImage() { cb(2,0,0); }
};
extern "C" __declspec(dllexport) void __cdecl blink_present(double c,int live,int w,int h,
 double curve,double soft,double blur,int sampling,Callback cb) {
 GLBackend b{cb}; present(c,live!=0,w,h,curve,soft,blur,sampling!=0,b);
}
#else
int main() {
 for(bool live:{false,true}) {
  Counter b; present(0.,live,3840,2160,.35,.02,.4,true,b);
  assert(b.allocations==0 && b.binds==0 && b.draws==0 && b.copies==0);
 }
 Counter excluded; present(.5,false,3840,2160,.35,.02,.4,true,excluded);
 assert(excluded.draws==0 && excluded.binds==0 && excluded.allocations==0);
 Counter dark; present(.5,true,3840,2160,.35,.02,0.,false,dark);
 assert(dark.draws==1 && dark.binds==1 && dark.allocations==0 && dark.copies==0);
 Counter blur; present(.1,true,3840,2160,.35,.02,.4,true,blur);
 assert(blur.draws==1 && blur.binds==1 && blur.allocations==1 && blur.copies==2 && blur.rows<400);
 Counter full; present(1.,true,3840,2160,1.,1.,1.,true,full);
 assert(full.copies==1 && full.rows==2160);
 std::cout << "PASS production admission wrapper: idle 0 alloc/bind/draw/copy; Dark00 1 draw/0 copy; blur two lid bands; full bands merge\n";
}
#endif
'''


def admission(compiler):
    src=OUT/'pov_blink_admission.cpp'; src.write_text(ADMISSION,encoding='utf8')
    def compile_run(include, name, expect=True):
        exe=OUT/(name+'.exe')
        subprocess.run([compiler,'/nologo','/std:c++17','/EHsc','/W4','/WX',
                        '/I'+str(include),str(src),'/Fe:'+str(exe),'/Fo:'+str(OUT/(name+'.obj'))],check=True)
        run=subprocess.run([str(exe)],capture_output=True,text=True)
        assert (run.returncode==0)==expect, (name,run.stdout,run.stderr)
        if expect: print(run.stdout.strip())
    compile_run(ROOT/'indra/newview','pov_admission')
    mutant=OUT/'idle_mutant';mutant.mkdir(exist_ok=True)
    header=(ROOT/'indra/newview/alpovblinkmath.h').read_text(encoding='utf8')
    guard='if (!shouldDraw(amount, live) || width <= 0 || height <= 0) return;'
    assert header.count(guard)==1
    (mutant/'alpovblinkmath.h').write_text(header.replace(guard,'(void)live;'),encoding='utf8')
    compile_run(mutant,'pov_idle_mutant',False)
    print('PASS negative control: removing production idle admission fails executable test')
    dll=OUT/'pov_blink_bridge.dll'
    subprocess.run([compiler,'/nologo','/std:c++17','/EHsc','/LD','/DBRIDGE',
                    '/I'+str(ROOT/'indra/newview'),str(src),'/Fe:'+str(dll),
                    '/Fo:'+str(OUT/'pov_bridge.obj'),'/link','/IMPLIB:'+str(OUT/'pov_bridge.lib')],check=True)


def integration_contract():
    display=(ROOT/'indra/newview/llviewerdisplay.cpp').read_text(encoding='utf8')
    start=display.index('void render_ui(F32 zoom_factor, int subfield)')
    ui=display[start:display.index('\nvoid swap()',start)]
    order=['gPipeline.renderFinalize();','gatherFrame();','render_hud_attachments();',
           'LLActorMover::instance().renderStudioGhosts();','ALPOVBlink::renderLastSceneImage(',
           'bool render_ui =','render_ui_3d();','render_ui_2d();']
    assert [ui.index(x) for x in order]==sorted(ui.index(x) for x in order)
    assert display.count('ALPOVBlink::renderLastSceneImage(')==1
    call=ui[ui.index('ALPOVBlink::renderLastSceneImage('):ui.index('bool render_ui =')]
    assert all(flag in call for flag in ('!gSnapshot','!gCubeSnapshot','!LLPipeline::sPrismLensRender',
        '!LLPipeline::sPrismPreviewRender','!LLPipeline::sReflectionRender','!LLPipeline::sImpostorRender',
        '!LLPipeline::sRenderingHUDs','gPipeline.mRT == &gPipeline.mMainRT'))
    cpp=(ROOT/'indra/newview/alpovblink.cpp').read_text(encoding='utf8')
    assert 'ALPOVBlinkMath::present(' in cpp and 'bindTarget(' not in cpp
    assert 'LLGLDepthTest depth(GL_FALSE, GL_FALSE)' in cpp
    assert 'LLRender::BF_ZERO, LLRender::BF_ONE' in cpp and 'TFO_POINT' in cpp
    cleanup=cpp[cpp.index('    void release()'):cpp.index('    U32 getWidth()')]
    assert 'bind' not in cleanup and 'LLImageGL::deleteTextures' in cleanup
    assert 'LLRenderTarget sample_target' not in cpp
    pipe=(ROOT/'indra/newview/pipeline.cpp').read_text(encoding='utf8')
    assert 'beginPresentation' not in pipe and 'endPresentation' not in pipe
    # Retain the same-frame ordering audit, but never infer next-frame safety
    # from it: sensor FXAA bypasses scratch prep and debug reads it anyway.
    assert 'LLRenderTarget& aa_map = mSensorPhysicalOrder ? *src : mFXAAMap;' in pipe
    aa=pipe[pipe.index('LLRenderTarget& aa_map ='):pipe.index('LLRenderTarget& aa_map =')+400]
    assert 'if (!mSensorPhysicalOrder)' in aa
    debug=pipe[pipe.index('void LLPipeline::renderFinalizePresent'):]
    assert 'mFXAAMap' in debug
    after=pipe[pipe.index('    // [RelightQ] Show the current selected channel'):pipe.index('void LLPipeline::',pipe.index('    // [RelightQ] Show the current selected channel'))]
    assert 'mFXAAMap' not in after
    readers=[]
    for p in (ROOT/'indra').rglob('*.cpp'):
        if 'mFXAAMap' in p.read_text(encoding='utf8',errors='replace'):
            readers.append(p.name)
    assert sorted(readers)==['pipeline.cpp'],readers
    assert 'mFXAAMap' not in call and 'scratch->' not in cpp
    conflict=(ROOT/'indra/newview/llkeyconflict.cpp').read_text(encoding='utf8')
    save=conflict[conflict.index('void LLKeyConflictHandler::saveToSettings('):]
    # WP5 advances the shared migration marker to 5 for Ctrl+Alt+K while
    # retaining blink/hot-gaze migrations before the edited mode is spliced.
    assert save.index('LLViewerInput::loadKeysXML(filename, keys)')<save.index('switch (mLoadMode)')<save.index('setS32("KeyBindingsMigrationVersion", 5)')
    print('PASS real render_ui order: finalize/gather/HUD/ghosts/blink/UI; no redirection; depth/alpha preserved; AA scratch never borrowed; later debug/capture cannot observe blink copies')


class Runner:
    def __init__(self, mutation=None):
        self.g=g=G.GL(); pr=g.program()
        for file,stage in [('postDeferredNoTCV.glsl',0x8B31),('povBlinkF.glsl',0x8B30)]:
            source=(ROOT/'indra/newview/app_settings/shaders/class1/deferred'/file).read_text(encoding='utf8')
            if file=='povBlinkF.glsl' and mutation=='flat':
                source=source.replace('float arch = blink_shape.y * x*x / (1.0 + x*x);','float arch = 0.0;')
            if file=='povBlinkF.glsl' and mutation=='outside':
                source=source.replace('if (mask <= 0.0) discard;','mask = max(mask, .01);')
            if file=='povBlinkF.glsl' and mutation=='double_mask':
                source=source.replace('lid = clamp(lid + (image-scene)*(1.0-blink_shape.w)*(1.0-c), 0.0, 1.0);','lid = mix(image, lid, mask);')
            sh=g.create_shader(stage); raw=('#version 420\n'+source).encode(); text=C.c_char_p(raw)
            g.source(sh,1,C.byref(text),None);g.compile(sh);ok=g.I();g.getshader(sh,0x8B81,C.byref(ok))
            if not ok.value:
                log=C.create_string_buffer(64000);g.slog(sh,len(log),None,log);raise AssertionError(log.value.decode())
            g.attach(pr,sh)
        g.link(pr);ok=g.I();g.getprog(pr,0x8B82,C.byref(ok));assert ok.value
        self.pr=pr;g.use(pr)
        self.uniform=lambda name,*v:g.uf[len(v)](g.loc(pr,name.encode()),*v)
        for cap in (0x0B71,0x0BE2,0x0C11,0x8DB9,0x0B44,0x0BD0):g.disable(cap)
        g.fn('glDepthMask',None,g.U)(0)
        self.blend=g.fn('glBlendFuncSeparate',None,g.U,g.U,g.U,g.U)
        self.upload=g.fn('glTexSubImage2D',None,g.U,g.I,g.I,g.I,g.I,g.I,g.U,g.U,C.c_void_p)
        self.copy=g.fn('glCopyTexSubImage2D',None,g.U,g.I,g.I,g.I,g.I,g.I,g.I,g.I)
        self.fbo,self.vbo=g.U(),g.U();g.gen_fbo(1,C.byref(self.fbo));g.bind_fbo(0x8D40,self.fbo)
        g.gen_buf(1,C.byref(self.vbo));g.bind_buf(0x8892,self.vbo)
        verts=np.array([[-1.,-1.,0.],[3.,-1.,0.],[-1.,3.,0.]],np.float32)
        g.buffer_data(0x8892,verts.nbytes,verts.ctypes.data,0x88E4)
        attr=g.attr_loc(pr,b'position');g.enable_attr(attr);g.attr_ptr(attr,3,0x1406,0,12,None)
        self.textures=[]
        self.bridge=C.CDLL(str(OUT/'pov_blink_bridge.dll'))
        self.callback_type=C.CFUNCTYPE(None,C.c_int,C.c_int,C.c_int)
        self.bridge.blink_present.argtypes=[C.c_double,C.c_int,C.c_int,C.c_int,C.c_double,C.c_double,C.c_double,C.c_int,self.callback_type]

    def texture(self,pixels,w,h,fmt=0x8814,external=0x1908):
        g=self.g;tex=g.U();g.gen_tex(1,C.byref(tex));self.textures.append(tex);g.bind_tex(0x0DE1,tex)
        g.teximage(0x0DE1,0,fmt,w,h,0,external,0x1406,None if pixels is None else pixels.ctypes.data)
        for key in (0x2800,0x2801):g.param(0x0DE1,key,0x2600) # point
        for key in (0x2802,0x2803):g.param(0x0DE1,key,0x812F)
        return tex

    def setup(self,w,h,patterned=False,bits=8):
        g=self.g;g.use(self.pr);g.bind_fbo(0x8D40,self.fbo)
        for tex in self.textures:g.del_tex(1,C.byref(tex))
        self.textures=[];self.w,self.h=w,h
        y,x=np.mgrid[:h,:w];self.x,self.y=(x+.5)/w,(y+.5)/h
        checker=((x//max(1,w//30)+y//max(1,h//18))%2).astype(np.float32)
        pixels=np.ones((h,w,4),np.float32)
        if patterned:
            pixels[:,:,0]=.12+.6*checker;pixels[:,:,1]=.15+.65*x/w;pixels[:,:,2]=.12+.65*y/h
        pixels[:,:,3]=.2+.6*x/w
        # Model actual final-present quantisation, then Studio ghost rectangles.
        # Ordering is independently taken from / asserted against real render_ui.
        self.levels=(1<<bits)-1
        pixels=np.round(pixels*self.levels)/self.levels
        pixels[h//5:h//3,w//4:w//2,:3]=[.8,.4,.2] if patterned else [1.,1.,1.]
        self.pixels=pixels
        self.target=self.texture(pixels,w,h)
        g.fbo_tex(0x8D40,0x8CE0,0x0DE1,self.target,0)
        self.depth_pixels=np.ascontiguousarray(.1+.7*(x+2*y)/(w+2*h),np.float32)
        depth=self.texture(self.depth_pixels,w,h,0x8CAC,0x1902)
        g.fbo_tex(0x8D40,0x8D00,0x0DE1,depth,0)
        self.source=self.texture(None,w,h)
        assert g.fbo_status(0x8D40)==0x8CD5
        g.ui(g.loc(self.pr,b'diffuseMap'),0);g.ui(g.loc(self.pr,b'uDitherBits'),bits);g.viewport(0,0,w,h)
        self.uniform('screen_res',float(w),float(h))

    def draw(self,closure,style,curve=.35,softness=.02,dark=.35,blur=.4,read=True):
        g=self.g;g.use(self.pr);g.active(0x84C0)
        # Start from final present + ghosts every frame, never from old lids.
        g.bind_tex(0x0DE1,self.target);self.upload(0x0DE1,0,0,0,self.w,self.h,0x1908,0x1406,self.pixels.ctypes.data)
        sampling=style==1 or dark>0 or blur>0
        self.counts={'allocations':0,'binds':0,'draws':0,'copies':0};self.bands=[]
        self.uniform('blink_shape',closure,curve,softness,float(style));self.uniform('blink_look',dark,blur)
        g.ui(g.loc(self.pr,b'blink_sampling'),int(sampling))
        def op(code,a,b):
            if code==0:
                # Preallocated fixture texture; production fallback policy is tested above.
                g.bind_tex(0x0DE1,self.source)
                poison=np.full((self.h,self.w,4),-100.,np.float32)
                self.upload(0x0DE1,0,0,0,self.w,self.h,0x1908,0x1406,poison.ctypes.data)
            elif code==3:
                self.counts['copies']+=1;self.bands.append((a,b))
                self.copy(0x0DE1,0,0,a,0,a,self.w,b)
            elif code==1:
                self.counts['binds']+=1
                if sampling:g.bind_tex(0x0DE1,self.source)
            elif code==2:
                self.counts['draws']+=1
                g.disable(0x0B71);g.fn('glDepthMask',None,g.U)(0)
                g.fn('glEnable',None,g.U)(0x0BE2)
                self.blend(0x0302,0x0303,0,1);g.draw(0x0004,0,3)
        cb=self.callback_type(op)
        self.bridge.blink_present(closure,1,self.w,self.h,curve,softness,blur,int(sampling),cb)
        if read:
            pixels=np.zeros((self.h,self.w,4),np.float32)
            g.read(0,0,self.w,self.h,0x1908,0x1406,pixels.ctypes.data);assert np.isfinite(pixels).all()
            depth=np.zeros((self.h,self.w),np.float32)
            g.read(0,0,self.w,self.h,0x1902,0x1406,depth.ctypes.data)
            assert np.array_equal(depth,self.depth_pixels),'Spatial depth changed (orientation/offset/write)'
            assert np.array_equal(pixels[:,:,3],self.pixels[:,:,3]),'Destination alpha changed'
            return pixels

    def timing(self,closure,style):
        # Time only draw submission, not Python reset/readback/copy orchestration.
        g=self.g;q=g.U();g.gen_queries(1,C.byref(q))
        self.draw(closure,style,read=False);g.finish();g.begin_query(0x88BF,q)
        for _ in range(30):g.draw(0x0004,0,3)
        g.end_query(0x88BF);g.finish();elapsed=C.c_uint64();g.query_u64(q,0x8866,C.byref(elapsed))
        return elapsed.value/30/1e6


def expected_mask(r,c,curve,soft):
    f=max(soft*.15,1/r.h);x=(r.x-.5)*r.w/r.h
    a=curve*x*x/(1+x*x)*c*(1-c)
    top=1+f-(.58+2*f)*c-a;bottom=-f+(.42+2*f)*c+a
    d=np.minimum(top-r.y,r.y-bottom);v=np.clip((d+f)/(2*f),0,1)
    return 1-v*v*(3-2*v)


def spatial(r):
    for w,h in ((384,216),(504,216),(256,256)):
        r.setup(w,h)
        for curve in (0.,.35,1.):
            for soft in (0.,.02,1.):
                closed=r.draw(1.,0,curve,soft,dark=0.,blur=0.)
                assert np.max(np.abs(closed[:,:,:3]-[.004,.003,.005]))<1e-6
                partial=r.draw(.5,0,curve,soft,dark=0.,blur=0.)
                mask=expected_mask(r,.5,curve,soft)
                expected=r.pixels[:,:,:3]*(1-mask[:,:,None])+np.array([.004,.003,.005])*mask[:,:,None]
                assert np.max(np.abs(partial[:,:,:3]-expected))<1e-4,('Curved, oriented, aspect-correct mask mismatch',w,h,curve,soft,float(np.max(np.abs(partial[:,:,:3]-expected))))
                # Force sampling independently of the no-sample oracle: a uniform
                # white scene has zero blur delta, so the entire coverage oracle
                # remains exact even at maximum Blur.
                sampled=r.draw(.5,0,curve,soft,dark=0.,blur=1.)
                assert np.max(np.abs(sampled[:,:,:3]-expected))<1e-4,'Sampled full mask oracle'
                sampled_closed=r.draw(1.,0,curve,soft,dark=0.,blur=1.)
                assert np.max(np.abs(sampled_closed[:,:,:3]-[.004,.003,.005]))<1e-6
                # Both edge rows, asymmetric seam, and off-axis curvature matter.
                assert partial[int(h*.46),w//2,:3].min()>.9
        for bits in (8,10):
            r.setup(w,h,True,bits)
            for c in (0.,1e-6,.001,.1,.5):
                p=r.draw(c,1,blur=1.)
                gap=expected_mask(r,c,.35,.02)==0
                assert np.array_equal(p[gap],r.pixels[gap]),'Untouched pixels changed'
                if c==0:assert r.counts==dict(allocations=0,binds=0,draws=0,copies=0)
                else:
                    copied=np.zeros(h,bool)
                    for y,n in r.bands:copied[y:y+n]=True
                    covered=(expected_mask(r,c,.35,.02)>0).any(axis=1)
                    # Positive subpixel closure can lie between pixel centres.
                    covered[[0,-1]]=True
                    assert np.all(copied[covered]),'Covered row not copied'
                    pad=int(np.ceil(1+10*c))
                    dilation=np.convolve(covered.astype(int),np.ones(2*pad+3,int),'same')>0
                    assert np.all(~copied|dilation),'Copy outside padded lid bands'
        closed=r.draw(1.,0,dark=.35,blur=1.)
        assert np.max(np.abs(closed[:,:,:3]-[.004,.003,.005]))<1e-6,'Sampled Dark full closure must stay opaque'
        for curve in (0.,.35,1.):
            for soft in (.02,.4,1.):
                baseline=r.draw(.5,0,curve,soft,dark=0.,blur=0.)
                for dark,blur in ((1e-6,0.),(0.,1e-6),(1e-6,1e-6)):
                    sampled=r.draw(.5,0,curve,soft,dark=dark,blur=blur)
                    assert np.max(np.abs(sampled-baseline))<2e-6,'Darken/Blur zero continuity'
        r.draw(.5,0,dark=0.,blur=0.);assert r.counts==dict(allocations=0,binds=1,draws=1,copies=0)
    print('PASS GL wrapper: curve/orientation/aspect mask oracle; spatial depth and alpha unchanged; 8/10-bit present+ghost gap bit-exact at closure 0+; band-only copies')


def production_present_chain():
    """Render actual final-blit + shared post effects into 8/10-bit destinations."""
    r=Runner();g=r.g
    shader_root=ROOT/'indra/newview/app_settings/shaders/class1'
    util=(shader_root/'deferred/deferredUtil.glsl').read_text(encoding='utf8')
    a=util.index('vec3 clampHDRRange(vec3 color)\n{');b=util.index('\n}',a)+2
    clamp=util[a:b]
    for bits in (8,10):
        for sensor in (False,True):
            pr=g.program()
            sources=[(0x8B31,(shader_root/'deferred/postDeferredNoTCV.glsl').read_text(encoding='utf8')),
                     (0x8B30,(shader_root/'alchemy/blitWithEffectsF.glsl').read_text(encoding='utf8')),
                     (0x8B30,(shader_root/'alchemy/postEffectUtilsF.glsl').read_text(encoding='utf8')),
                     (0x8B30,clamp)]
            for stage,source in sources:
                sh=g.create_shader(stage)
                raw=('#version 420\n#define DITHER 1\n'+('#define AL_SENSOR_HDR 1\n' if sensor else '')+source).encode()
                text=C.c_char_p(raw);g.source(sh,1,C.byref(text),None);g.compile(sh)
                ok=g.I();g.getshader(sh,0x8B81,C.byref(ok))
                if not ok.value:
                    log=C.create_string_buffer(64000);g.slog(sh,len(log),None,log);raise AssertionError(log.value.decode())
                g.attach(pr,sh)
            g.link(pr);ok=g.I();g.getprog(pr,0x8B82,C.byref(ok))
            if not ok.value:
                log=C.create_string_buffer(64000);g.plog(pr,len(log),None,log);raise AssertionError(log.value.decode())
            r.setup(384,216,True,bits)
            g.use(pr)
            # Replace the colour destination with a real quantising attachment.
            r.target=r.texture(None,r.w,r.h,0x8058 if bits==8 else 0x8059)
            g.fbo_tex(0x8D40,0x8CE0,0x0DE1,r.target,0)
            scene=r.pixels.copy();scene[:,:,0]=np.linspace(-.1,1.1,r.w,dtype=np.float32)
            scene[:,:,1]=np.linspace(.605,.607,r.w,dtype=np.float32)
            scene[:,:,2]=np.linspace(.652,.654,r.h,dtype=np.float32)[:,None]
            source=r.texture(scene,r.w,r.h)
            depth=r.texture(r.depth_pixels,r.w,r.h,0x8CAC,0x1902)
            g.active(0x84C0);g.bind_tex(0x0DE1,source);g.ui(g.loc(pr,b'diffuseRect'),0)
            g.active(0x84C1);g.bind_tex(0x0DE1,depth);g.ui(g.loc(pr,b'depthMap'),1)
            g.ui(g.loc(pr,b'diopterWarpMap'),0)
            g.ui(g.loc(pr,b'uDitherBits'),bits);g.uf[1](g.loc(pr,b'uDitherAmount'),1.)
            g.uf[2](g.loc(pr,b'uResolution'),float(r.w),float(r.h))
            g.disable(0x0BE2);g.fn('glEnable',None,g.U)(0x0B71)
            g.fn('glDepthMask',None,g.U)(1);g.fn('glDepthFunc',None,g.U)(0x0207)
            attr=g.attr_loc(pr,b'position');g.enable_attr(attr);g.attr_ptr(attr,3,0x1406,0,12,None)
            g.draw(0x0004,0,3)
            # Scene-dressing stand-in at the real ghost stage, after final dither.
            ghost=np.full((24,64,4),[.8,.4,.2,.6],np.float32)
            g.active(0x84C0);g.bind_tex(0x0DE1,r.target)
            r.upload(0x0DE1,0,100,30,64,24,0x1908,0x1406,ghost.ctypes.data)
            r.pixels=np.empty((r.h,r.w,4),np.float32)
            g.read(0,0,r.w,r.h,0x1908,0x1406,r.pixels.ctypes.data)
            g.read(0,0,r.w,r.h,0x1902,0x1406,r.depth_pixels.ctypes.data)
            g.use(r.pr);attr=g.attr_loc(r.pr,b'position');g.enable_attr(attr);g.attr_ptr(attr,3,0x1406,0,12,None)
            for c in (0.,1e-6,.001,.1,.5):
                for style,dark,blur in ((0,0.,0.),(0,.5,1.),(1,.5,1.)):
                    p=r.draw(c,style,dark=dark,blur=blur)
                    gap=expected_mask(r,c,.35,.02)==0
                    assert np.array_equal(p[gap],r.pixels[gap]),(bits,sensor,c,'real final-blit gap changed')
                    if c==0:assert np.array_equal(p,r.pixels)
            closed=r.draw(1.,0,dark=0.,blur=0.)
            assert closed[30:54,100:164,:3].max()<.01,'Ghost stage escaped closed lids'
    print('PASS production final-blit + postEffectUtils + ghost stage + production blink wrapper: real RGBA8/RGB10_A2 quantisation/dither, legacy/sensor permutations, closure 0+ untouched bit-exact')


def render():
    integration_contract()
    production_present_chain()
    r=Runner();spatial(r)
    for mutation in ('flat','outside','double_mask'):
        try:spatial(Runner(mutation))
        except AssertionError:print('PASS negative control:',mutation,'fails GL assertions')
        else:raise AssertionError('Mutation survived: '+mutation)
    # Restore production program / context after mutant checks.
    r=Runner();r.setup(3840,2160,True)
    renders=OUT/'renders';renders.mkdir(exist_ok=True)
    metrics={'renderer':r.g.renderer,'resolution':[3840,2160],'gpu_ms_per_draw':{},'scope':'synthetic overlay draw only; no viewer or allocation/copy timing'}
    for style,name in enumerate(('Dark','Warm')):
        for c in (0.,.5,1.):
            p=r.draw(c,style)
            if c==0:assert np.array_equal(p,r.pixels)
            if c==1 and style==1:
                assert .11<p[:,:,0].mean()<.17 and p[:,:,0].std()>.001
            Image.fromarray((np.clip(p[::-1,:,:3],0,1)*255+.5).astype(np.uint8)).save(renders/f'{name.lower()}_{c:g}.png')
            if c>0:metrics['gpu_ms_per_draw'][f'{name}_{c:g}']=r.timing(c,style)
    (OUT/'render_metrics.json').write_text(json.dumps(metrics,indent=2)+'\n',encoding='utf8')
    print(json.dumps(metrics,indent=2));print('PASS six updated 4K PNGs')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-compiler',default='cl.exe');parser.add_argument('--gl-only',action='store_true')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    if not args.gl_only:native(args.native_compiler)
    render()


if __name__=='__main__':main()

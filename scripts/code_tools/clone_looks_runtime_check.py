"""Execute production Studio look policy, motion and copy code with GL state spies.

The WGL companion renders real shaders; this isolates the viewer controller so
an unused Python imitation cannot assert that feature-off does no viewer work.
"""
from pathlib import Path
import os
import subprocess

from relight_area_policy_check import method


def check():
    path = Path('indra/newview/llactormover.cpp')
    source = path.read_text(encoding='utf-8')
    overlay = method(path, 'void LLActorMover::renderStudioGhosts(')
    draw = method(path, 'S32 drawGeometryGhost(')
    assert 'studio_scene_needed(item.mInst->mStyle, item.mInst->mAlpha, phase2_visible(item))' in overlay
    assert overlay.count('studio_scene_copy(look_frame.mViewport)') == 1
    assert overlay.index('studio_scene_copy(') < overlay.index('drawGeometryGhost(')
    assert 'studio_displaced_sweep(style, studio_frame != nullptr,' in draw
    assert 'if (displaced_sweep > 0.f)' in draw
    displaced = draw.split('if (displaced_sweep > 0.f)',1)[1].split('// Shared live Actor FX beauty',1)[0]
    for required in ('LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL)',
                     'LLRender::BF_ZERO, LLRender::BF_ONE',
                     'draw_batches(SWEEP_SOLID, true, false, true)',
                     'draw_static(SWEEP_SOLID, true, false, true)',
                     'studio_displace = 0.f', 'apply_program(static_shader)',
                     'apply_program(shader)', 'gGL.blendFunc(cs, cd, as, ad)',
                     'saved_shader->bind()', 'ScopedStudioTexture texture(0)'):
        assert required in displaced, required
    for required in ('item.mInst->mFrozenBoundsRadius', 'item.mInst->mCadenceBoundsRadius',
                     'pivot = item.mInst->mFrozenFootAgent', 'pivot = item.mInst->mCadenceFootAgent',
                     'if (it->second.mFrame != frame) it = motion.erase(it)',
                     'studio_screen_velocity(previous, frame, item.mAv->getID()'):
        assert required in overlay, required
    studio = Path('indra/newview/alghoststudio.cpp').read_text(encoding='utf-8')
    assert 'inst->mFrozenBoundsRadius = clone_capture_radius(av, foot)' in studio
    assert 'inst->mCadenceBoundsRadius = clone_capture_radius(av, inst->mCadenceFootAgent)' in studio
    fixture = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
using S32=int; using U32=unsigned; using F32=float; using F64=double; using GLint=int;
bool llfinite(float x){return std::isfinite(x);}
template<class T>T llmax(T a,T b){return std::max(a,b);}
template<class T>T llclamp(T v,T a,T b){return std::clamp(v,a,b);}
struct LLUUID { int value=0; bool operator==(LLUUID b)const{return value==b.value;} };
enum {VX,VY,VZ};
const F32 GHOST_SCALE_MIN=.01f,GHOST_SCALE_MAX=100.f;
struct LLVector3 {
 float mV[3];LLVector3(float x=0,float y=0,float z=0):mV{x,y,z}{}
 LLVector3 operator+(LLVector3 b)const{return {mV[0]+b.mV[0],mV[1]+b.mV[1],mV[2]+b.mV[2]};}
 LLVector3 operator-(LLVector3 b)const{return {mV[0]-b.mV[0],mV[1]-b.mV[1],mV[2]-b.mV[2]};}
 LLVector3 operator*(float s)const{return {mV[0]*s,mV[1]*s,mV[2]*s};}
 float length()const{return std::sqrt(mV[0]*mV[0]+mV[1]*mV[1]+mV[2]*mV[2]);}
 bool isFinite()const{return std::isfinite(mV[0])&&std::isfinite(mV[1])&&std::isfinite(mV[2]);}
};
struct Root {LLVector3 pos;LLVector3 getWorldPosition(){return pos;}};
struct LLVOAvatar {
 Root root;LLVector3 ext[2]={{-.5f,-.5f,0},{.5f,.5f,2}};
 Root* getRootJoint(){return &root;}const LLVector3* getLastAnimExtents(){return ext;}
 float getPelvisToFoot(){return 0;}
};
struct ALGhostStudio {
 enum {POSE_LIVE,POSE_FROZEN};
 struct Instance {
  float mAlpha=1,mScale=1,mPoseRateHz=0,mFrozenBoundsRadius=2,mCadenceBoundsRadius=2;
  int mPose=POSE_LIVE;std::vector<int> mFrozenPalettes,mCadencePalettes;
  LLVector3 mFrozenFootAgent,mCadenceFootAgent;
 };
};
struct StudioItem {const ALGhostStudio::Instance* mInst;LLVOAvatar* mAv;LLVector3 mFootAgent;};
struct LLViewerCamera {
 static LLViewerCamera* getInstance(){static LLViewerCamera camera;return &camera;}
 bool sphereInFrustum(LLVector3 foot,float radius){return foot.mV[0]+radius>=0;}
};
struct LLVector2 {
 float mV[2]={}; LLVector2(float x=0,float y=0):mV{x,y}{}
 LLVector2 operator-(LLVector2 b)const{return {mV[0]-b.mV[0],mV[1]-b.mV[1]};}
 float length()const{return std::hypot(mV[0],mV[1]);}
 void clear(){mV[0]=mV[1]=0;} void operator*=(float s){mV[0]*=s;mV[1]*=s;}
};
struct LLVector3d {
 double x=0,y=0,z=0;
 LLVector3d operator-(LLVector3d b)const{return {x-b.x,y-b.y,z-b.z};}
 double lengthSquared()const{return x*x+y*y+z*z;}
};
enum {GHOST_STYLE_GLASS=64,GHOST_STYLE_ASH=68,GHOST_STYLE_ARC=72};
enum {GL_TEXTURE_BINDING_2D=1,GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,
 GL_READ_BUFFER,GL_DRAW_BUFFER0,GL_READ_FRAMEBUFFER,GL_TEXTURE_2D,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE};
#define LL_PROFILE_ZONE_NAMED_CATEGORY_UI(x)
#define LL_PROFILE_GPU_ZONE(x)
U32 frame=1;
struct LLFrameTimer { static U32 getFrameCount(){return frame;} };
int draw_fbo=7,read_fbo=2,draw_buffer=103,copies=0,allocations=0;
std::map<int,int> read_buffers={{7,101},{2,102}};
std::array<int,8> binding2d={};
int copied_source=0,copied_buffer=0,copy_w=0,copy_h=0;
struct LLTexUnit;
struct Render {
 U32 active=3; std::array<LLTexUnit*,8> units;
 U32 getCurrentTexUnitIndex(){return active;} LLTexUnit* getTexUnit(U32 n){return units[n];}
 void flush(){}
} gGL;
struct LLTexUnit {
 enum eTextureType {TT_NONE,TT_TEXTURE,TT_CUBE_MAP,TT_RECT_TEXTURE};
 enum {TFO_BILINEAR,TAM_CLAMP};
 U32 index; eTextureType type=TT_NONE; U32 texture=0; bool mips=false;
 void activate(){gGL.active=index;}
 eTextureType getCurrType(){return type;} U32 getCurrTexture(){return texture;}
 bool hasMipMaps(){return mips;} void setHasMipMaps(bool b){mips=b;}
 void unbind(eTextureType t){activate();if(type==t){texture=0;if(t==TT_TEXTURE)binding2d[index]=999;}}
 void disable(){if(type!=TT_NONE){unbind(type);type=TT_NONE;}}
 void enable(eTextureType t){if(type!=t && t!=TT_NONE){disable();type=t;}}
 bool bindManual(eTextureType t,U32 tex,bool mip=false){
  if(texture!=tex){activate();enable(t);texture=tex;mips=mip;if(t==TT_TEXTURE)binding2d[index]=tex;}return true;
 }
 void setTextureFilteringOption(int){} void setTextureAddressMode(int){}
};
void glGetIntegerv(int name,int* value){
 switch(name){case GL_TEXTURE_BINDING_2D:*value=binding2d[gGL.active];break;
 case GL_DRAW_FRAMEBUFFER_BINDING:*value=draw_fbo;break;
 case GL_READ_FRAMEBUFFER_BINDING:*value=read_fbo;break;
 case GL_READ_BUFFER:*value=read_buffers[read_fbo];break;
 case GL_DRAW_BUFFER0:*value=draw_buffer;break;default:assert(false);}
}
void glBindTexture(int target,int tex){assert(target==GL_TEXTURE_2D);binding2d[gGL.active]=tex;}
void glBindFramebuffer(int target,int fbo){assert(target==GL_READ_FRAMEBUFFER);read_fbo=fbo;}
void glReadBuffer(int buffer){read_buffers[read_fbo]=buffer;}
void glCopyTexSubImage2D(int,int,int,int,int,int,int w,int h){
 ++copies;copied_source=read_fbo;copied_buffer=read_buffers[read_fbo];copy_w=w;copy_h=h;
 assert(binding2d[gGL.active]!=7); // independent destination, not source attachment
}
struct LLImageGL {
 int w=0,h=0;U32 name=0;
 LLImageGL(bool,bool){} bool getHasGLTexture(){return name!=0;}
 int getWidth(){return w;} int getHeight(){return h;}
 void setSize(int x,int y,int){w=x;h=y;} bool createGLTexture(){name=500+allocations++;return true;}
 U32 getTexName(){return name;}
 static void setManualImage(int,int,int,int,int,int,int,void*,bool){}
};
template<class T>struct LLPointer {
 std::shared_ptr<T> p;operator bool()const{return bool(p);} T* get(){return p.get();}
 T* operator->(){return p.get();} void operator=(T* v){p.reset(v);}
};
'''
    for signature in ('bool studio_scene_needed(', 'F32 studio_displaced_sweep(',
                      'struct StudioMotion', 'LLVector2 studio_screen_velocity(',
                      'class ScopedStudioTexture', 'LLImageGL* studio_scene_copy('):
        fixture += method(path, signature) + (';' if signature.startswith(('class','struct')) else '') + '\n'
    fixture += method(path,'const auto phase2_visible = []') + ';\n'
    fixture += method(Path('indra/newview/alghoststudio.cpp'),'static F32 clone_capture_radius(') + '\n'
    fixture += r'''
int main(){
 std::array<LLTexUnit,8> units;
 for(U32 i=0;i<8;++i){units[i].index=i;gGL.units[i]=&units[i];}
 GLint viewport[4]={17,23,3840,2160};
 // Exact texture cache, hidden 2D binding, mip flag and active unit restore.
 for(auto type:{LLTexUnit::TT_NONE,LLTexUnit::TT_TEXTURE,LLTexUnit::TT_CUBE_MAP,LLTexUnit::TT_RECT_TEXTURE})
 for(U32 texture:{0U,41U}){
  if(type==LLTexUnit::TT_NONE && texture)continue; // impossible LLRender cache state
  auto& unit=units[1];unit.type=type;unit.texture=texture;unit.mips=true;binding2d[1]=83;gGL.active=3;
  {ScopedStudioTexture guard(1);unit.bindManual(LLTexUnit::TT_TEXTURE,501);}
  assert(unit.type==type && unit.texture==texture && unit.mips && binding2d[1]==83 && gGL.active==3);
 }
 // Execute actual policy gates and scene copy with no selected feature.
 for(int look=0;look<78;++look){
  if(look==64 || look==68 || look==72)continue;
  assert(!studio_scene_needed(look,1,true));
  assert(studio_displaced_sweep(look,true,1,false,true)==0);
 }
 assert(!studio_scene_needed(64,0,true) && !studio_scene_needed(64,1,false));
 for(int look:{68,72}){
  assert(studio_displaced_sweep(look,false,1,false,true)==0);
  assert(studio_displaced_sweep(look,true,0,false,true)==0);
  assert(studio_displaced_sweep(look,true,1,true,true)==0);
  assert(studio_displaced_sweep(look,true,1,false,false)==0);
  assert(studio_displaced_sweep(look,true,1,false,true)>0);
 }
 assert(copies==0 && allocations==0);
 auto* scene=studio_scene_copy(viewport);
 assert(scene && copies==1 && allocations==1 && copy_w==3840 && copy_h==2160);
 assert(copied_source==7 && copied_buffer==103 && read_fbo==2);
 assert(read_buffers[7]==101 && read_buffers[2]==102 && gGL.active==3);
 assert(studio_scene_copy(viewport)==scene && copies==1); // all Glass clones share
 GLint other[4]={0,0,1920,1080};
 assert(studio_scene_copy(other)==nullptr && copies==1); // never stale cross-view
 ++frame;assert(studio_scene_copy(viewport)==scene && copies==2 && allocations==1);
 ++frame;assert(studio_scene_copy(other) && copies==3 && allocations==2);
 // Consecutive root motion, still fallback, clamp, teleport, respawn, gaps,
 // pause, invalid projection, resized or second view, independent instances.
 StudioMotion motion,independent;LLVOAvatar av,respawn;LLUUID id{1};
 auto velocity=[&](U32 f,float x,float y,LLVector3d foot=LLVector3d{},
                   const LLVOAvatar* avatar=nullptr,bool valid=true,double time=-1){
  return studio_screen_velocity(motion,f,id,avatar?avatar:&av,time<0?f/60.:time,foot,{x,y},viewport,valid);
 };
 assert(velocity(10,0,0).length()==0);
 auto v=velocity(11,12,-5);assert(v.mV[0]==12 && v.mV[1]==-5);
 assert(velocity(11,200,200).mV[0]==12); // once per frame
 assert(studio_screen_velocity(motion,11,id,&av,11/60.,{}, {},other,true).length()==0);
 assert(velocity(12,12,-5).length()==0);
 assert(std::abs(velocity(13,112,-5).length()-48)<.001);
 assert(velocity(14,2000,-5).length()==0); // camera cut
 assert(velocity(15,2001,-5,{20,0,0}).length()==0); // world teleport
 assert(velocity(16,2002,-5,{20,0,0},&respawn).length()==0);
 assert(velocity(18,2003,-5,{20,0,0},&respawn).length()==0);
 assert(velocity(19,2004,-5,{20,0,0},&respawn,true,9).length()==0);
 assert(velocity(20,2005,-5,{},&av,false).length()==0);
 assert(velocity(21,2006,-5).length()==0);
 assert(studio_screen_velocity(independent,22,id,&av,1,{}, {500,0},viewport,true).length()==0);
 // Captured giant/spread pose with offscreen feet must remain visible after
 // the live source shrinks its pose. The same rule covers stepped cadence.
 ALGhostStudio::Instance inst;StudioItem item{&inst,&av,{-10,0,0}};
 assert(!phase2_visible(item));
 av.ext[0]={-15,-1,-1};av.ext[1]={15,1,2};
 inst.mFrozenBoundsRadius=clone_capture_radius(&av,{});
 av.ext[0]={-.5f,-.5f,0};av.ext[1]={.5f,.5f,2};
 inst.mPose=ALGhostStudio::POSE_FROZEN;inst.mFrozenPalettes={1};assert(phase2_visible(item));
 inst.mPose=ALGhostStudio::POSE_LIVE;inst.mPoseRateHz=2;inst.mCadencePalettes={1};
 inst.mCadenceBoundsRadius=inst.mFrozenBoundsRadius;assert(phase2_visible(item));
 inst.mAlpha=0;assert(!phase2_visible(item));
 std::cout<<"PASS production controller: feature-off copy/sweep zero; shared 4K copy + state restore; motion clamps/resets/identity/views\n";
}
'''
    directory = Path('.tmp/looks2_20261102/runtime').resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (directory/'runtime.cpp').write_text(fixture, encoding='utf-8')
    vcvars = next((Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    (directory/'compile.cmd').write_text(
        f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /std:c++17 /EHsc '
        f'/Fo"{directory/"runtime.obj"}" /Fe"{directory/"runtime.exe"}" "{directory/"runtime.cpp"}"\n'
        'exit /b %ERRORLEVEL%\n', encoding='utf-8')
    subprocess.run(['cmd','/d','/c',str(directory/'compile.cmd')],check=True)
    subprocess.run([str(directory/'runtime.exe')],check=True)


if __name__ == '__main__':
    check()

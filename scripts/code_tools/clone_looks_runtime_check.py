"""Execute the real Studio scheduling, uniform, motion and copy helpers with spies.

Negative controls compile mutated production orchestration and require the same
feature-off assertions to fail. WGL shader/image parity remains a separate gate.
"""
from pathlib import Path
import json
import os
import subprocess
from relight_area_policy_check import method


def check():
    path = Path('indra/newview/llactormover.cpp')
    overlay = method(path, 'void LLActorMover::renderStudioGhosts(')
    draw = method(path, 'S32 drawGeometryGhost(')
    # Matrix ids must be reachable from every real selector and persistence,
    # and must take the existing TEXTURED solid range dispatch.
    import re
    import xml.etree.ElementTree as ET
    source = path.read_text(encoding='utf-8')
    names = ('DECODE', 'GLYPH_SKIN', 'CONSTRUCT_WIRE', 'DEJA_VU', 'OPERATOR', 'DIAL_OUT', 'SOURCE')
    for value, name in enumerate(names, 78):
        assert re.search(r'GHOST_STYLE_' + name + r'\s*=\s*' + str(value) + r'\b', source)
    assert 'GHOST_STYLE_STUDIO_END = 85' in source
    assert 'style >= GHOST_STYLE_STUDIO_BASE && style < GHOST_STYLE_STUDIO_END' in draw
    assert '? static_cast<S32>(GHOST_STYLE_THERMAL) : style' in draw
    assert 'sh->uniform1i(sGhostLook, style)' in draw
    assert 'const LLVector2 ghost_screen_size = gp.mWorldLinear' in draw
    assert '? LLRenderPass::actorFxScreenSize()' in draw
    assert ': (matrix_screen ? actor_ghost_screen_size() : LLVector2(1.f, 1.f))' in draw
    assert 'sh->uniform2fv(sGhostScreenSize, 1, ghost_screen_size.mV)' in draw
    heading = method(path, 'void LLActorMover::renderHeadingPreview(')
    assert 'ALGhostStudio::resolveOverlayStyle((S32)ghost_style)' in heading
    assert 'ALGhostStudio::isOverlayStyle' in Path('indra/newview/alghoststudio.cpp').read_text(encoding='utf-8') or 'isOverlayStyle(' in Path('indra/newview/alghoststudio.cpp').read_text(encoding='utf-8')
    assert '(style >= 64 && style <= 84 && style != 68 && style != 72)' in Path('indra/newview/alghoststudio.h').read_text(encoding='utf-8')
    xui = Path('indra/newview/skins/default/xui/en')
    selectors = 0
    for file in ('panel_ghost_studio.xml', 'panel_dpage_cast_ghosts.xml', 'panel_path_editor.xml', 'panel_dpage_cast_path.xml'):
        for combo in ET.parse(xui/file).iter('combo_box'):
            if combo.get('name') in ('style_combo','ghost_style_combo') or combo.get('name','').startswith('crowd_mix_look_'):
                values = [n.get('value') for n in combo.findall('combo_box.item')]
                assert all(values.count(str(i)) == 1 for i in range(78,85)), (file,combo.get('name'),values)
                selectors += 1
    assert selectors == 16, selectors
    for look in range(78,85):
        assert f'case {look}: return ' in Path('indra/newview/alpanelghoststudio.cpp').read_text(encoding='utf-8')
    # Existing CPU clock and per-instance UUID seed reach the shared upload.
    assert 'actor_ghost_studio_frame_time(gp.mEffectFps)' in draw
    assert 'gp.mEffectFps        = inst.mEffectFps' in overlay
    assert 'inst.mId.mData[0] | (inst.mId.mData[1] << 8)' in overlay
    assert '++invocation;' in overlay
    assert 'studio_prepare_item(inst.mStyle, inst.mAlpha, phase2,' in overlay
    assert 'studio_draw_item(inst.mStyle, inst.mAlpha, instance_frame.has_value(),' in overlay
    assert overlay.count('studio_scene_copy(') == 1
    assert overlay.index('std::sort(items.begin()') < overlay.index('studio_scene_copy(') < overlay.index('drawGeometryGhost(')
    # Visibility/fade filtering must precede helper resolution and all Glass
    # preparation: zero presentation coverage must never copy the scene.
    gather_guard = overlay[overlay.index('for (ALGhostStudio::Instance& inst : studio.getInstances())'):
                           overlay.index('LLVOAvatar* av = ALGhostStudio::resolveOverlayAvatar(inst);')]
    assert '!studio.isVisible(inst)' in gather_guard
    assert '|| !(inst.mFadeAlpha > 0.f)' in gather_guard
    assert gather_guard.index('!(inst.mFadeAlpha > 0.f)') < gather_guard.index('continue;')
    assert 'studio_apply_phase2(sh, studio_frame);' in draw
    assert 'if (!sh || !sh->mProgramObject) return;' in draw
    for obsolete in ('ghostDisplace', 'studio_displaced_sweep', 'studio_draw_displaced',
                     'studio_reset_loaded', 'sGhostWorldUp'):
        assert obsolete not in path.read_text(), obsolete
    assert 'std::any_of' not in overlay
    assert 'auto& scene_caches = studio_scene_caches();' in overlay
    assert 'view->mTake = LLCinematicCamera::instance().takeGeneration();' in overlay
    assert 'view->mCameraOwner = static_cast<S32>(gCameraDispatchOwner);' in overlay
    cine = Path('indra/newview/llcinematiccamera.cpp').read_text()
    reset = cine.index('if (fresh_activation || mode_changed || target_changed || serial_changed ||')
    assert reset < cine.index('++mTakeGeneration;', reset) < cine.index('mPhase = 0.f;', reset)
    assert cine.count('++mTakeGeneration;') == 1
    assert 'U64 takeGeneration() const { return mTakeGeneration; }' in Path('indra/newview/llcinematiccamera.h').read_text()
    for signature in ('bool LLViewerWindow::rawSnapshot(', 'bool LLViewerWindow::simpleSnapshot('):
        capture_path = method(Path('indra/newview/llviewerwindow.cpp'), signature)
        assert capture_path.index('scratch_space.release();') < capture_path.index('LLActorMover::releaseStudioSnapshotScratch();')
    assert 'LLActorMover::releaseStudioSnapshotScratch();' in method(Path('indra/newview/llfloater360capture.cpp'), 'void LLFloater360Capture::capture360Images(')
    assert 'studio_scene_caches().endCapture();' in method(path, 'void LLActorMover::releaseStudioSnapshotScratch(')
    idle = method(path, 'StudioSceneCaches& studio_scene_caches(')
    assert 'gIdleCallbacks.addFunction' in idle
    assert 'studio_scene_caches().expire(LLTimer::getTotalSeconds());' in idle
    assert 'gSnapshot' not in idle
    assert overlay.index('scene_caches.begin(gSnapshot, pose_now)') < overlay.index('if (!studio.anyEnabled())')
    assert 'scene_caches.get(gSnapshot, look_frame->mViewport)' in overlay
    studio = Path('indra/newview/alghoststudio.cpp').read_text()
    # Clock upload is outside all held-pose/fade/helper dispatch. Pose cadence
    # must change geometry sampling, never the independent creative FX clock.
    assert 'actor_ghost_studio_frame_time(gp.mEffectFps)' in draw
    assert 'sh->uniform1f(sGhostTime, ghost_now);' in draw
    for field in ('gp.mEffectFps        = inst.mEffectFps;',
                  'gp.mFrozenPalettes = &inst.mOffsetPose->mFrozenPalettes;',
                  'gp.mFrozenPalettes = &inst.mCadencePalettes;',
                  'gp.mFrozenPalettes = &inst.mFrozenPalettes;'):
        assert field in overlay, field
    assert 'remapRetiredOverlayStyle(gSavedSettings.getS32(' in studio
    assert 'gSavedSettings.setS32(name, look);' in studio
    assert 'ALGhostStudio::resolveOverlayStyle((S32)ghost_style)' in heading
    import xml.etree.ElementTree as ET
    xml = ET.parse('indra/newview/skins/default/xui/en/panel_ghost_studio.xml')
    for name in ['style_combo'] + ['crowd_mix_look_'+str(i) for i in range(1,7)]:
        choices = xml.find(".//*[@name='"+name+"']")
        ids = [int(item.get('value')) for item in choices.findall('combo_box.item')]
        assert 68 not in ids and 72 not in ids and 69 in ids and 77 in ids
    assert 'inst->mCadenceBoundsRadius = need_bounds' in studio
    assert '(!need_bounds || inst->mCadenceBoundsRadius > 0.f)' in studio
    assert 'GHOST_SCENE_MAP,' in Path('indra/llrender/llshadermgr.h').read_text()
    assert 'mReservedUniforms.push_back("ghostSceneMap")' in Path('indra/llrender/llshadermgr.cpp').read_text()
    fixture = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <optional>
#include <cstdlib>
#undef assert
#define assert(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " << #x << "\n"; std::exit(3); } } while (0)
using U64=unsigned long long; using S32=int; using U32=unsigned; using F32=float; using F64=double; using GLint=int;
bool llfinite(float x){return std::isfinite(x);}
template<class T>T llmin(T a,T b){return std::min(a,b);}
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
 static const LLVector3 zero;
 void set(float x,float y,float z){mV[0]=x;mV[1]=y;mV[2]=z;}
 void clear(){set(0,0,0);}
 float lengthSquared()const{return mV[0]*mV[0]+mV[1]*mV[1]+mV[2]*mV[2];}
 float length()const{return std::sqrt(mV[0]*mV[0]+mV[1]*mV[1]+mV[2]*mV[2]);}
 bool isFinite()const{return std::isfinite(mV[0])&&std::isfinite(mV[1])&&std::isfinite(mV[2]);}
};
const LLVector3 LLVector3::zero;
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
  float mStartOffset=0;
  int mPose=POSE_LIVE;std::vector<int> mFrozenPalettes,mCadencePalettes;
  LLVector3 mFrozenFootAgent,mCadenceFootAgent;
  LLVector3 mFrozenBounds[2]={{-.5f,-.5f,0},{.5f,.5f,2}};
  LLVector3 mCadenceBounds[2]={{-.5f,-.5f,0},{.5f,.5f,2}};
  std::shared_ptr<const Instance> mOffsetPose;
 };
};
struct StudioItem {const ALGhostStudio::Instance* mInst;LLVOAvatar* mAv;LLVector3 mFootAgent;};
struct LLViewerCamera {
 static LLViewerCamera* getInstance(){static LLViewerCamera camera;return &camera;}
 bool sphereInFrustum(LLVector3 foot,float radius){return foot.mV[0]+radius>=0;}
};
struct LLVector2 {
 float mV[2]={}; LLVector2(float x=0,float y=0):mV{x,y}{}
 LLVector2 operator+(LLVector2 b)const{return {mV[0]+b.mV[0],mV[1]+b.mV[1]};}
 LLVector2 operator*(float s)const{return {mV[0]*s,mV[1]*s};}
 LLVector2 operator-(LLVector2 b)const{return {mV[0]-b.mV[0],mV[1]-b.mV[1]};}
 float length()const{return std::hypot(mV[0],mV[1]);}
 void clear(){mV[0]=mV[1]=0;} void operator*=(float s){mV[0]*=s;mV[1]*=s;}
};
struct LLVector3d {
 double x=0,y=0,z=0;
 LLVector3d operator-(LLVector3d b)const{return {x-b.x,y-b.y,z-b.z};}
 double lengthSquared()const{return x*x+y*y+z*z;}
};
enum {GHOST_STYLE_GLASS=64,GHOST_STYLE_DATAMOSH=75};
enum {GL_TEXTURE_BINDING_2D=1,GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,
 GL_NONE=0, GL_DRAW_FRAMEBUFFER=11, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, GL_READ_BUFFER,GL_DRAW_BUFFER0,GL_READ_FRAMEBUFFER,GL_TEXTURE_2D,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE};
#define LL_PROFILE_ZONE_NAMED_CATEGORY_UI(x)
#define LL_PROFILE_GPU_ZONE(x)
U32 frame=1;
bool gSnapshot=true; // display() can leave this latched after capture
struct LLTimer {static F64 now;static F64 getTotalSeconds(){return now;}};
F64 LLTimer::now=0;
struct IdleCallbacks {void (*callback)(void*)=nullptr;void addFunction(void (*fn)(void*)){callback=fn;}} gIdleCallbacks;
U32 gFrameCount=0;
const U32 U32_MAX=~0U;
F64 elapsed=100.;
struct LLFrameTimer { static U32 getFrameCount(){return frame;} static F64 getElapsedSeconds(){return elapsed;} };
int draw_fbo=7,read_fbo=2,draw_buffer=103,copies=0,allocations=0,resident=0,retired=0;
std::map<int,int> read_buffers={{7,101},{2,102}};
std::array<int,8> binding2d={};
int copied_source=0,copied_buffer=0,copy_w=0,copy_h=0,copy_x=0,copy_y=0,source_x=0,source_y=0;
int attachment_type=1,red_bits=8;
void glGetFramebufferAttachmentParameteriv(int,int,int name,int* out){*out=name==GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE?red_bits:attachment_type;}
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
void glCopyTexSubImage2D(int,int,int x,int y,int sx,int sy,int w,int h){
 copy_x=x;copy_y=y;source_x=sx;source_y=sy;
 ++copies;copied_source=read_fbo;copied_buffer=read_buffers[read_fbo];copy_w=w;copy_h=h;
 assert(binding2d[gGL.active]!=7); // independent destination, not source attachment
}
struct LLImageGL {
 int w=0,h=0;U32 name=0;
 ~LLImageGL(){if(name)++retired;} // production queues deletion; storage survives until idle
 static void updateClass(){resident-=retired;retired=0;}
 LLImageGL(bool,bool){} bool getHasGLTexture(){return name!=0;}
 int getWidth(){return w;} int getHeight(){return h;}
 void setSize(int x,int y,int){w=x;h=y;} bool createGLTexture(){name=500+allocations++;++resident;return true;}
 U32 getTexName(){return name;}
 static void setManualImage(int,int,int,int,int,int,int,void*,bool){}
};
template<class T>struct LLPointer {
 std::shared_ptr<T> p;operator bool()const{return bool(p);} T* get(){return p.get();}
 T* operator->(){return p.get();} void operator=(T* v){p.reset(v);}
};

namespace glm {
struct vec4 {float x,y,z,w;vec4(float a=0,float b=0,float c=0,float d=0):x(a),y(b),z(c),w(d){}float operator[](int a)const{return (&x)[a];}};
struct mat4 {
 float m[4][4]={}; explicit mat4(float d=1){for(int i=0;i<4;++i)m[i][i]=d;}
 bool operator==(const mat4& b)const{for(int i=0;i<4;++i)for(int j=0;j<4;++j)if(m[i][j]!=b.m[i][j])return false;return true;}
};
vec4 operator*(const mat4& a,vec4 b){vec4 r{};for(int i=0;i<4;++i)for(int j=0;j<4;++j)(&r.x)[i]+=a.m[i][j]*b[j];return r;}
}
int uniforms=0,binds=0;
struct LLGLSLShader {
 U32 mProgramObject=0;
 void bind(){assert(mProgramObject);++binds;}
 void uniform1f(int,float){++uniforms;} void uniform1i(int,int){++uniforms;}
 void uniform2f(int,float,float){++uniforms;} void uniform3fv(int,int,const float*){++uniforms;}
} gActorGhostProgram;
int sGhostVelocity,sGhostSceneValid,sGhostScreenSize,sGhostScreenOrigin;
'''
    for signature in ('struct StudioLookFrame', 'bool studio_phase2_look(', 'bool studio_scene_needed(',
                      'struct StudioView', 'bool studio_same_view(',
                      'struct StudioMotion', 'LLVector2 studio_screen_velocity(',
                      'template<class Visible, class Prepare>',
                      'template<class Copy, class Draw>',
                      'void studio_apply_phase2(', 'class ScopedStudioTexture',
                      'struct StudioSceneCache', 'struct StudioSceneCaches', 'StudioSceneCaches& studio_scene_caches(', 'std::array<GLint, 4> studio_glass_rect(',
                      'LLImageGL* studio_scene_copy('):
        fixture += method(path, signature) + (';' if signature.startswith(('class','struct')) else '') + '\n'
    fixture += method(path, 'const auto phase2_radius =') + ';\n'
    fixture += method(path, 'F32 actor_ghost_studio_frame_time(') + '\n'
    header = Path('indra/newview/alghoststudio.h')
    for signature in ('static S32 remapRetiredOverlayStyle(', 'static bool isOverlayStyle(',
                      'static S32 resolveOverlayStyle('):
        # Execute the exact dependency-free production policy as free functions.
        fixture += method(header, signature) + '\n'
    fixture += r'''

int main(){
 assert(resolveOverlayStyle(68)==1 && resolveOverlayStyle(72)==1);
 assert(!isOverlayStyle(68) && !isOverlayStyle(72));
 for(int id=0;id<78;++id)if(isOverlayStyle(id))assert(resolveOverlayStyle(id)==id);
 assert(remapRetiredOverlayStyle(-1)==-1); // empty crowd menu remains empty
 assert(llclamp(remapRetiredOverlayStyle(68),0,27)==1);
 assert(llclamp(remapRetiredOverlayStyle(72),0,27)==1);
 assert(llclamp(remapRetiredOverlayStyle(99),0,27)==27); // existing invalid-path policy
 // Real production clock: smooth, stepped, same-frame held time, snapshot
 // tiles/faces, then normal-display resume. gSnapshot resets in display().
 gSnapshot=false;
 assert(actor_ghost_studio_frame_time(0)==0);
 ++gFrameCount;elapsed=100.04;
 assert(std::abs(actor_ghost_studio_frame_time(0)-.04f)<1e-5);
 assert(actor_ghost_studio_frame_time(8)==0);
 elapsed=101.;assert(std::abs(actor_ghost_studio_frame_time(0)-.04f)<1e-5);
 ++gFrameCount;elapsed=100.14;
 assert(std::abs(actor_ghost_studio_frame_time(8)-.125f)<1e-5);
 gSnapshot=true;++gFrameCount;elapsed=100.2;
 assert(std::abs(actor_ghost_studio_frame_time(0)-.2f)<1e-5);
 ++gFrameCount;elapsed=100.7;
 assert(std::abs(actor_ghost_studio_frame_time(0)-.2f)<1e-5);
 gSnapshot=false;++gFrameCount;elapsed=100.8;
 assert(std::abs(actor_ghost_studio_frame_time(0)-.8f)<1e-5);
 gSnapshot=true; // remainder exercises cache expiry despite latched display flag
 std::array<LLTexUnit,8> units;
 for(U32 i=0;i<8;++i){units[i].index=i;gGL.units[i]=&units[i];}
 GLint viewport[4]={17,23,3840,2160};
 // Execute the actual radius lambda against delayed capture-time extents.
 // A historical non-phase2 cadence capture may have radius zero before the
 // look changes to Glass; its paired extents must still keep it onscreen.
 LLVOAvatar bounds_avatar;
 ALGhostStudio::Instance held_instance;
 auto delayed_pose=std::make_shared<ALGhostStudio::Instance>();
 delayed_pose->mFrozenBoundsRadius=0;
 delayed_pose->mFrozenFootAgent={100,0,0};
 delayed_pose->mFrozenBounds[0]={80,-1,0};
 delayed_pose->mFrozenBounds[1]={120,1,30};
 held_instance.mStartOffset=1;
 held_instance.mOffsetPose=delayed_pose;
 held_instance.mPoseRateHz=12;held_instance.mCadencePalettes={1};
 StudioItem bounds_item{&held_instance,&bounds_avatar,{-25,0,0}};
 const float delayed_radius=phase2_radius(bounds_item);
 const float expected_radius=15.f+std::sqrt(2504.f)*.5f+.8f;
 assert(std::abs(delayed_radius-expected_radius)<.0001f);
 auto delayed_frame=studio_prepare_item(64,1,true,[&](){
  return LLViewerCamera::getInstance()->sphereInFrustum(bounds_item.mFootAgent,phase2_radius(bounds_item));
 },[](){return StudioLookFrame{};});
 assert(delayed_frame); // live and current cadence bounds are both too small
 held_instance.mPose=ALGhostStudio::POSE_FROZEN;held_instance.mFrozenPalettes={1};
 assert(phase2_radius(bounds_item)<5); // frozen takes precedence over offset
 held_instance.mPose=ALGhostStudio::POSE_LIVE;held_instance.mOffsetPose.reset();
 held_instance.mCadenceBounds[0]=delayed_pose->mFrozenBounds[0];
 held_instance.mCadenceBounds[1]=delayed_pose->mFrozenBounds[1];
 held_instance.mCadenceFootAgent=delayed_pose->mFrozenFootAgent;
 held_instance.mCadenceBoundsRadius=0;
 assert(std::abs(phase2_radius(bounds_item)-expected_radius)<.0001f);
 held_instance.mPoseRateHz=0;
 assert(!LLViewerCamera::getInstance()->sphereInFrustum(bounds_item.mFootAgent,phase2_radius(bounds_item)));
 for(auto type:{LLTexUnit::TT_NONE,LLTexUnit::TT_TEXTURE,LLTexUnit::TT_CUBE_MAP,LLTexUnit::TT_RECT_TEXTURE})
 for(U32 texture:{0U,41U}){
  if(type==LLTexUnit::TT_NONE && texture)continue;
  auto& unit=units[1];unit.type=type;unit.texture=texture;unit.mips=true;binding2d[1]=83;gGL.active=3;
  {ScopedStudioTexture guard(1);unit.bindManual(LLTexUnit::TT_TEXTURE,501);}
  assert(unit.type==type && unit.texture==texture && unit.mips && binding2d[1]==83 && gGL.active==3);
 }
 StudioSceneCache cache;U64 invocation=1;int frames=0,radii=0,draws=0;
 LLGLSLShader rigged;rigged.mProgramObject=2;gActorGhostProgram.mProgramObject=1;
 auto run=[&](int look,float alpha,bool visible,bool active){
  auto inputs=studio_prepare_item(look,alpha,active,[&](){++radii;return visible;},[&](){
   ++frames;StudioLookFrame f;std::copy(viewport,viewport+4,f.mViewport);f.mInvocation=invocation;return f;});
  studio_draw_item(look,alpha,visible,inputs?&*inputs:nullptr,
   [&](){return studio_scene_copy(cache,invocation,1.,viewport,{200,300,800,900});},
   [&](const StudioLookFrame* f){
    ++draws;rigged.bind();studio_apply_phase2(&rigged,f);
   });
 };
 // Actual production lazy preparation -> copy -> draw -> uniform -> sweep chain.
 for(int look=0;look<78;++look)if(!studio_phase2_look(look))run(look,1,true,false);
 assert(copies==0 && allocations==0 && frames==0 && radii==0 && uniforms==0);
 for(int look:{64,75}){run(look,0,true,true);run(look,1,false,true);}
 assert(copies==0 && frames==0 && uniforms==0 && radii==2);
 radii=0;
 // Only Glass and Datamosh allocate transient inputs; retired ids do not.
 run(68,1,true,true);run(72,1,true,true);
 assert(frames==0 && radii==0 && uniforms==0);
 run(75,1,true,true);
 assert(frames==1 && radii==1 && uniforms==4 && copies==0);
 // Leaving an active state resets once; steady inactive draws issue no setters.
 int active_uniforms=uniforms;run(65,1,true,false);assert(uniforms==active_uniforms+4);
 int reset_uniforms=uniforms;for(int i=0;i<20;++i)run(65,1,true,false);
 assert(uniforms==reset_uniforms);
 gActorGhostProgram.mProgramObject=1;
 run(64,1,true,true);auto* scene=cache.mScene.get();
 assert(scene && copies==1 && allocations==1 && copy_w==800 && copy_h==900);
 assert(copy_x==200 && copy_y==300 && source_x==217 && source_y==323);
 assert(copied_source==7 && copied_buffer==103 && read_fbo==2);
 assert(read_buffers[7]==101 && read_buffers[2]==102 && gGL.active==3);
 run(64,1,true,true);assert(copies==2 && cache.mScene.get()==scene); // rear Glass now included
 ++invocation;run(64,1,true,true);assert(copies==3 && cache.mInvocation==invocation); // same FBO + viewport, new render
 int old_copies=copies,old_allocations=allocations;draw_buffer=GL_NONE;
 assert(!studio_scene_copy(cache,++invocation,2.,viewport,{0,0,10,10}));
 assert(copies==old_copies && allocations==old_allocations && cache.mInvocation==0);
 draw_buffer=103;attachment_type=GL_NONE;
 assert(!studio_scene_copy(cache,++invocation,2.,viewport,{0,0,10,10}));
 assert(copies==old_copies && allocations==old_allocations);attachment_type=1;
 red_bits=0;assert(!studio_scene_copy(cache,++invocation,2.,viewport,{0,0,10,10}));
 assert(copies==old_copies && allocations==old_allocations);red_bits=8;
 cache.expire(3.9);assert(cache.mScene);cache.expire(4.1);assert(!cache.mScene);
 assert(retired==1 && resident==1);LLImageGL::updateClass();assert(resident==0);
 // No idle/updateClass occurs during equal-size capture passes. Group two
 // sizes, six faces x three passes each, and two Glass copies per invocation.
 StudioSceneCaches capture;const int capture_allocations=allocations,capture_copies=copies;
 GLint small[4]={0,0,2048,2048};
 U32 names[2]={};
 for(int size=0;size<2;++size)for(int face=0;face<6;++face)for(int pass=0;pass<3;++pass){
  const auto& vp=size?viewport:small;capture.begin(true,5.);
  auto& scratch=capture.get(true,vp);++invocation;
  for(int glass=0;glass<2;++glass){
   auto* image=studio_scene_copy(scratch,invocation,5.,vp,{0,0,10,10});assert(image);
   if(!names[size])names[size]=image->getTexName();assert(names[size]==image->getTexName());
  }
  assert(allocations-capture_allocations<=2 && resident<=2 && retired<=1);
  assert(capture.mSnapshot.size()==1);
 }
 assert(allocations==capture_allocations+2 && copies==capture_copies+72 && resident==2);
 capture.endCapture(); // completed capture, no next live invocation
 assert(capture.mSnapshot.empty() && retired==2 && resident==2);
 LLImageGL::updateClass();assert(resident==0 && retired==0);
 // Completion and idle release have no dependency on live begin/render.
 for(int size=1;size<=5;++size){
  GLint vp[4]={0,0,512*size,512*size};capture.begin(true,6.);
  studio_scene_copy(capture.get(true,vp),++invocation,6.,vp,{0,0,10,10});
  assert(capture.mSnapshot.size()==1);
  LLImageGL::updateClass();assert(resident==1 && retired==0);
 }
 capture.expire(8.9);assert(capture.mSnapshot.size()==1);
 capture.expire(9.1);assert(capture.mSnapshot.empty() && retired==1);
 LLImageGL::updateClass();assert(resident==0);
 // Execute the registered production idle callback with snapshot still
 // latched true and no live begin/render after the last snapshot.
 auto& abandoned=studio_scene_caches();LLTimer::now=10.;
 studio_scene_copy(abandoned.get(true,small),++invocation,10.,small,{0,0,10,10});
 assert(gIdleCallbacks.callback && gSnapshot);
 LLTimer::now=13.1;gIdleCallbacks.callback(nullptr);
 assert(abandoned.mSnapshot.empty() && retired==1);
 LLImageGL::updateClass();assert(resident==0);
 // Keep the earlier next-live-before-disabled/empty cleanup assertion too.
 studio_scene_copy(capture.get(true,small),++invocation,10.,small,{0,0,10,10});
 capture.begin(false,10.1);assert(capture.mSnapshot.empty() && retired==1);
 LLImageGL::updateClass();assert(resident==0);
 auto rect=studio_glass_rect({0,0,0},.1f,glm::mat4(1),viewport);
 assert(rect[0]<1728 && rect[1]<972 && rect[0]+rect[2]>2112 && rect[1]+rect[3]>1188);
 auto full=studio_glass_rect({0,0,0},3,glm::mat4(1),viewport);
 assert(full[0]==0 && full[1]==0 && full[2]==3840 && full[3]==2160);
 auto near=glm::mat4(1);near.m[3][3]=0;
 full=studio_glass_rect({},1,near,viewport);assert(full[2]==3840 && full[3]==2160);
 StudioMotion motion;LLVOAvatar av,respawn;LLUUID id{1};StudioView view;
 std::copy(viewport,viewport+4,view.mViewport);view.mAt={1,0,0};
 auto velocity=[&](U32 f,float x,float y){view.mFrame=f;view.mInvocation=f;return studio_screen_velocity(motion,view,id,&av,f/60.,{},{x,y},true);};
 assert(velocity(10,0,0).length()==0);
 auto v=velocity(11,12,-5);assert(v.mV[0]>1 && v.mV[1]<0 && v.length()<13);
 auto duplicate=velocity(11,200,200);assert(duplicate.mV[0]==v.mV[0]);
 view.mFbo=9;assert(velocity(11,200,200).length()==0);view.mFbo=0;
 // Same-size different camera/view in one frame cannot reuse or difference.
 view.mInvocation=12;view.mFrame=11;view.mOrdinal=1;
 assert(studio_screen_velocity(motion,view,id,&av,11/60.,{},{200,200},true).length()==0);
 assert(velocity(12,20,0).length()==0);view.mOrdinal=0;
 assert(velocity(13,21,0).length()==0);
 // Explicit cut under the old 216px threshold, plus transform-only backstop.
 ++view.mCut;assert(velocity(14,121,0).length()==0);
 velocity(15,122,0);velocity(16,123,0); // re-seed after cut
 view.mCamera={.6f,0,0};assert(velocity(17,124,0).length()==0);
 velocity(18,125,0);velocity(19,126,0);
 view.mAt={.998f,.07f,0};assert(velocity(20,127,0).length()==0);
 view.mTeleport=true;assert(velocity(21,128,0).length()==0);view.mTeleport=false;
 assert(velocity(23,129,0).length()==0); // gap
 // Subpixel noise never enters trails; sustained drift enters, noisy drift
 // stays active, and stillness exits once after smoothing (no strobe).
 motion=StudioMotion{};view=StudioView{};std::copy(viewport,viewport+4,view.mViewport);
 float x=0;for(int f=30;f<60;++f){x+=(f&1)?.2f:-.25f;assert(velocity(f,x,0).length()==0);}
 for(int f=60;f<80;++f){x+=2;v=velocity(f,x,0);}assert(v.length()>1);
 for(int f=80;f<110;++f){x+=(f&1)?.8f:1.2f;assert(velocity(f,x,0).length()>.5f);}
 int transitions=0;bool on=true;
 for(int f=110;f<140;++f){bool next=velocity(f,x,0).length()>0;if(next!=on)++transitions;on=next;}
 assert(!on && transitions==1);
 // The real 1.5% / 0.25Hz VCam breathing and Slow Zoom change the
 // projection every frame while a directional Datamosh subject keeps moving.
 for(bool zoom:{false,true}){
  motion=StudioMotion{};view=StudioView{};std::copy(viewport,viewport+4,view.mViewport);
  for(U32 f=1;f<=600;++f){
   const float t=f/60.f;
   const float fov=zoom?60.f-2.f*t:60.f*(1.f+.015f*std::sin(2.f*3.14159265f*.25f*t));
   view.mProjection.m[0][0]=view.mProjection.m[1][1]=1.f/std::tan(fov*3.14159265f/360.f);
   v=velocity(f,2.f*f,-.5f*f);
   if(f>3)assert(v.mV[0]>1 && v.mV[1]<0 && v.length()<=48);
  }
 }
 // 60 degrees/s pan plus 13 m/s dolly, at each cadence and with a
 // cadence change every frame. Prediction uses elapsed seconds, not frames.
 for(int fps:{24,30,60,0}){
  motion=StudioMotion{};view=StudioView{};std::copy(viewport,viewport+4,view.mViewport);
  double t=0;float sx=0;
  for(U32 f=1;f<=240;++f){
   const double dt=1./(fps?fps:(f%3==0?24:f%3==1?30:60));t+=dt;sx+=120.f*(float)dt;
   view.mFrame=f;view.mInvocation=f;view.mCamera={13.f*(float)t,0,0};
   view.mAt={std::cos((float)t*1.04719755f),std::sin((float)t*1.04719755f),0};
   v=studio_screen_velocity(motion,view,id,&av,t,{},{sx,0},true);
   if(f>2)assert(v.mV[0]>1 && v.length()<=48);
  }
  // Only 1px screen movement: the transform residual must catch this jump.
  ++view.mFrame;++view.mInvocation;view.mCamera.mV[0]+=10.f;
  assert(studio_screen_velocity(motion,view,id,&av,t+1./60.,{},{sx+1,0},true).length()==0);
 }
 // Same pass/FBO/pose, Slow Zoom's 33-degree lens -> Crash Zoom's
 // 60-degree lens, foot initially 100px from centre: only 48.7px jump.
 // Each semantic reset source is an independent identity component.
 for(int boundary=0;boundary<3;++boundary){
  motion=StudioMotion{};view=StudioView{};std::copy(viewport,viewport+4,view.mViewport);
  for(U32 f=1;f<=5;++f)velocity(f,100,0);
  view.mProjection.m[0][0]=1.7320508f;
  if(boundary==0)++view.mTake;
  if(boundary==1)++view.mCameraOwner;
  if(boundary==2)++view.mCut;
  assert(velocity(6,51.3057f,0).length()==0);
 }
 // First comparable pair after init, gap, or explicit cut. Translation and
 // angle probes stay below the screen jump threshold, then hold and recover.
 for(int invalidation=0;invalidation<3;++invalidation)for(bool angle:{false,true}){
  motion=StudioMotion{};view=StudioView{};std::copy(viewport,viewport+4,view.mViewport);
  U32 f=1;
  if(invalidation){
   for(;f<=5;++f)velocity(f,0,0);
   if(invalidation==1)f+=2;
   else ++view.mCut;
  }
  assert(velocity(f++,0,0).length()==0);
  if(angle)view.mAt={.998f,.07f,0};else view.mCamera={.6f,0,0};
  assert(velocity(f++,10,0).length()==0);
  assert(!motion.mCameraVelocityValid); // provisional must not be trained
  assert(velocity(f++,10,0).length()==0); // held jump fails confirmation
  for(int k=0;k<12;++k) v=velocity(f++,12+2.f*k,0);
  assert(v.mV[0]>1); // no permanent fallback
 }
 // Equal-size tiles/faces change projection on the same FBO; snapshot
 // history must reset even across consecutive application frames.
 motion=StudioMotion{};view=StudioView{};std::copy(viewport,viewport+4,view.mViewport);
 for(U32 f=1;f<8;++f)v=velocity(f,2.f*f,0);assert(v.length()>1);
 view.mSnapshot=true;assert(velocity(8,18,0).length()==0);
 view.mProjection.m[0][0]=2;assert(velocity(9,20,0).length()==0);
 view.mCameraId=5;view.mAt={0,1,0};assert(velocity(10,22,0).length()==0);
 view.mSnapshot=false;view.mOrdinal=1;assert(velocity(11,24,0).length()==0);
 view.mOrdinal=0;assert(velocity(12,26,0).length()==0);
 std::cout<<"PASS production orchestration: feature-off copy/allocation/uniform/frame/radius zero; reserved-id no-phase2; per-Glass subrect/new invocation/state restore/depth/expiry; view/cut/EMA/hysteresis; 10s breathing/zoom; 24/30/60/variable cadence pan+dolly and jump; tiles/faces/aux reset; bounded snapshot size, capture-end/idle release; manual lens take/owner reset; unseeded jump fallback; delayed giant bounds with zero stored radius and frozen/offset/cadence/live precedence\n";
}
'''
    directory = Path('.tmp/looks2_20261102/runtime').resolve()
    directory.mkdir(parents=True, exist_ok=True)
    vcvars = next((Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    # Discover the native-fixture compiler environment once for this run.
    # Every control still compiles fresh production source and runs its own exe.
    env_cmd = directory/'environment.cmd'
    env_cmd.write_text(f'@echo off\ncall "{vcvars}" >nul\nset\n')
    environment = os.environ.copy()
    env_result = subprocess.run(['cmd','/d','/c',str(env_cmd)],capture_output=True,text=True,check=True)
    for line in env_result.stdout.splitlines():
        key,separator,value=line.partition('=')
        if separator and key:environment[key]=value
    def execute(name, text, passing):
        cpp, exe = directory/(name+'.cpp'), directory/(name+'.exe')
        cpp.write_text(text, encoding='utf-8')
        cmd = directory/(name+'.cmd')
        cmd.write_text(f'@echo off\ncl /nologo /std:c++17 /EHsc '
                       f'/Fo"{directory/(name+".obj")}" /Fe"{exe}" "{cpp}"\nexit /b %ERRORLEVEL%\n')
        subprocess.run(['cmd','/d','/c',str(cmd)],check=True,env=environment)
        result = subprocess.run([str(exe)],capture_output=True,text=True)
        assert (result.returncode == 0) == passing, (name,result.stdout,result.stderr)
        print(result.stdout if passing else 'PASS negative control rejected: '+name+'; '+result.stderr.strip(),flush=True)
        return dict(name=name,exit=result.returncode,stdout=result.stdout,stderr=result.stderr)
    results = [execute('runtime',fixture,True)]
    for name,before,after in (
        ('frozen_clock', 'const F64 elapsed = llmax(sFrameClock - sEpoch, 0.0);',
         'const F64 elapsed = 0.0;'),
        ('retired_id_fallback', 'return style == 68 || style == 72 ? 1 : style;',
         'return style == 68 || style == 72 ? 0 : style;'),
        ('forced_copy_off','if (frame && studio_scene_needed(look, alpha, visible)) frame->mScene = copy();',
         'if (true) { auto* unexpected = copy(); if(frame) frame->mScene = unexpected; }'),
        ('projection_identity','&& std::equal(a.mViewport, a.mViewport + 4, b.mViewport);',
         '&& std::equal(a.mViewport, a.mViewport + 4, b.mViewport) && a.mProjection == b.mProjection;'),
        ('fixed_frame_cut','const bool camera_cut = view.mTeleport || view.mCut != previous.mView.mCut',
         'const bool camera_cut = (view.mCamera - previous.mView.mCamera).lengthSquared() > .25f || (view.mAt - previous.mView.mAt).lengthSquared() > .001218f || view.mTeleport || view.mCut != previous.mView.mCut'),
        ('snapshot_per_invocation','if (!snapshot) mSnapshot.clear();','mSnapshot.clear();'),
        ('manual_take_identity','a.mTake == b.mTake && a.mCameraOwner == b.mCameraOwner','a.mCameraOwner == b.mCameraOwner'),
        ('camera_owner_identity','a.mTake == b.mTake && a.mCameraOwner == b.mCameraOwner','a.mTake == b.mTake'),
        ('unseeded_guard','LLVector2 raw = unseeded_large ? LLVector2() : screen - previous.mScreen;',
         'LLVector2 raw = screen - previous.mScreen;'),
        ('capture_end_release','void endCapture() { mSnapshot.clear(); }','void endCapture() {}'),
        ('snapshot_size_bound','if (mSnapshot.find(size) == mSnapshot.end()) mSnapshot.clear();',
         'if (false) mSnapshot.clear();'),
        ('idle_latched_snapshot','studio_scene_caches().expire(LLTimer::getTotalSeconds());',
         'if (!gSnapshot) studio_scene_caches().expire(LLTimer::getTotalSeconds());'),
        ('snapshot_idle_expiry','if (!mSnapshot.empty() && now - mSnapshot.begin()->second.mLastUse > 3.0) endCapture();',
         'if (false) endCapture();'),
        ('delayed_bounds_ignored','else if (item.mInst->mStartOffset > 0.f && item.mInst->mOffsetPose)',
         'else if (false)')):
        assert fixture.count(before)==1
        results.append(execute(name,fixture.replace(before,after),False))
    clock = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
using F32=float;using F64=double;using U32=uint32_t;
constexpr U32 U32_MAX=UINT32_MAX;
template<class T>T llmax(T a,T b){return std::max(a,b);}
template<class T>T llclamp(T v,T a,T b){return std::clamp(v,a,b);}
U32 gFrameCount=0;bool gSnapshot=false;double now=100.;
struct LLFrameTimer{static double getElapsedSeconds(){return now;}};
'''+method(path, 'F32 actor_ghost_studio_frame_time(')+r'''
int main(){
 assert(actor_ghost_studio_frame_time(0)==0);
 ++gFrameCount;now=100.501;
 assert(std::abs(actor_ghost_studio_frame_time(0)-.501)<1e-6);
 assert(actor_ghost_studio_frame_time(12)==.5f);
 assert(actor_ghost_studio_frame_time(30)==.5f);
 now=100.54; // Other clones in SAME render frame share the captured clock.
 assert(std::abs(actor_ghost_studio_frame_time(0)-.501)<1e-6);
 assert(actor_ghost_studio_frame_time(12)==.5f);
 ++gFrameCount;
 assert(std::abs(actor_ghost_studio_frame_time(0)-.54)<1e-6);
 assert(actor_ghost_studio_frame_time(12)==.5f);
 assert(std::abs(actor_ghost_studio_frame_time(30)-16.f/30)<1e-6);
 ++gFrameCount;now=100.59;
 assert(std::abs(actor_ghost_studio_frame_time(12)-7.f/12)<1e-6);
 gSnapshot=true;++gFrameCount;now=101.;float frozen=actor_ghost_studio_frame_time(12);
 ++gFrameCount;now=105.;assert(actor_ghost_studio_frame_time(12)==frozen);
 gSnapshot=false;++gFrameCount;assert(actor_ghost_studio_frame_time(0)==5.f);
 std::cout<<"PASS production Studio Effect fps 0/12/30; per-clone rates, same-frame hold, frame advance, snapshot freeze\n";
}
'''
    results.append(execute('matrix_clock',clock,True))
    assert clock.count('floor(real_time * fps) / fps')==1
    results.append(execute('matrix_clock_no_step',clock.replace('floor(real_time * fps) / fps','real_time'),False))
    # Compile the complete REAL parameter struct and the exact apply_program
    # uploads. Export the spy's values to the GL fixture, rather than inventing
    # shader defaults in Runner.render. Studio/path both default-construct gp.
    params = method(Path('indra/newview/llactormover.h'), '    struct GhostDrawParams') + ';'
    uploads = '\n'.join(re.findall(
        r'sh->uniform1[fi]\(sGhost(?:DissolveProgress|DecodeScrub),[^;]+;', draw))
    assert uploads.count('sh->') == 2
    default_fixture = r'''
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
using F32=float;using S32=int;using U64=unsigned long long;using U8=unsigned char;
struct LLQuaternion{};struct LLVector3{};struct LLMatrix4{};
struct LLUUID{bool operator<(const LLUUID&)const{return false;}};
using GhostCoverageMask=int;const int GHOST_COVERAGE_NONE=0;
template<class T>T llclamp(T v,T a,T b){return std::clamp(v,a,b);}
''' + params + r'''
const int sGhostDissolveProgress=1,sGhostDecodeScrub=2;
struct UploadSpy{
 float dissolve=-99;int scrub=-99;
 void uniform1f(int key,float value){assert(key==1);dissolve=value;}
 void uniform1i(int key,int value){assert(key==2);scrub=value;}
};
UploadSpy upload(const GhostDrawParams& gp){UploadSpy spy;auto* sh=&spy;
''' + uploads + r'''
 return spy;
}
int main(){
 GhostDrawParams studio,path,live;
 for(auto* gp:{&studio,&path,&live}){
  assert(gp->mDissolveProgress==.5f);assert(!gp->mDecodeScrub);
  auto s=upload(*gp);assert(s.dissolve==.5f && s.scrub==0);
 }
 studio.mDecodeScrub=true;studio.mDissolveProgress=0;
 auto manual=upload(studio);assert(manual.scrub==1 && manual.dissolve==0);
 auto s=upload(path);
 std::cout<<"{\"ghostDissolveProgress\":"<<s.dissolve<<",\"ghostDecodeScrub\":"<<s.scrub<<"}\n";
}
'''
    defaults = execute('matrix_draw_defaults', default_fixture, True)
    (directory/'matrix_draw_defaults.json').write_text(json.dumps(json.loads(defaults['stdout']),indent=2))
    results.append(defaults)
    results.append(execute('matrix_default_scrub',default_fixture.replace('mDecodeScrub = false','mDecodeScrub = true'),False))
    snapshot = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
using F32=float;using S32=int;
enum {VX,VY};bool gSnapshot=false;
template<class T>T llmax(T a,T b){return std::max(a,b);}
int llceil(float x){return (int)std::ceil(x);}
struct LLVector2{float mV[2];LLVector2(float x=0,float y=0):mV{x,y}{}};
struct LLViewerCamera{
 float zoom=1;int sub=0;
 static LLViewerCamera* getInstance(){static LLViewerCamera c;return &c;}
 float getZoomFactor(){return zoom;}int getZoomSubRegion(){return sub;}
};
struct Window{int getWorldViewWidthRaw(){return 192;}int getWorldViewHeightRaw(){return 256;}};
Window window;Window* gViewerWindow=&window;
''' + method(path,'LLVector2 actor_ghost_frag_offset(') + method(path,'LLVector2 actor_ghost_screen_size(') + r'''
int main(){
 auto* c=LLViewerCamera::getInstance();
 for(float zoom:{1.f,2.f,2.4f}){
  c->zoom=zoom;gSnapshot=zoom>1;
  int tiles=gSnapshot?llceil(zoom):1;
  auto size=actor_ghost_screen_size();
  assert(size.mV[0]==192*zoom && size.mV[1]==256*zoom);
  for(int sub=0;sub<tiles*tiles;++sub){
   c->sub=sub;auto offset=actor_ghost_frag_offset();
   assert(offset.mV[0]==(sub%tiles)*192 && offset.mV[1]==(sub/tiles)*256);
  }
 }
 std::cout<<"PASS production whole-image dimensions and tile offsets: ordinary, 2x, noninteger zoom\n";
}
'''
    results.append(execute('matrix_snapshot_field',snapshot,True))
    results.append(execute('matrix_tile_size',snapshot.replace('? llmax(LLViewerCamera::getInstance()->getZoomFactor(), 1.f) : 1.f','? 1.f : 1.f'),False))
    results.append(execute('matrix_rounded_tile_size',snapshot.replace('? llmax(LLViewerCamera::getInstance()->getZoomFactor(), 1.f) : 1.f','? static_cast<F32>(llceil(LLViewerCamera::getInstance()->getZoomFactor())) : 1.f'),False))
    (directory/'results.json').write_text(json.dumps(results,indent=2))


if __name__ == '__main__':
    check()

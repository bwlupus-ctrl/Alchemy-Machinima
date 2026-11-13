#!/usr/bin/env python3
"""Native fixtures for the approved performance ports 1, 2 and 5; no viewer build.

Compiles extracted production routines against deterministic GL/viewer stubs,
uses the pinned base routines as OFF/upload oracles, and runs planted mutants.
Products live in .tmp/upperf/fixtures. No upstream sources are read here.
"""
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

from native_check_tools import ROOT, compile_run
from shader_load_check import function

BASE = '54d3537525e'
OUT = ROOT / '.tmp/upperf/fixtures'


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def base(path):
    return subprocess.check_output(['git', 'show', BASE + ':' + path], cwd=ROOT,
                                   text=True, encoding='utf-8')


COMMON = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using U8=uint8_t; using U32=uint32_t; using S32=int32_t;
using F32=float; using F64=double; using GLfloat=float; using GLint=int;
template<class T> T llmin(T a,T b){return std::min(a,b);}
template<class T> T llmax(T a,T b){return std::max(a,b);}
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY
#define LL_PROFILE_ZONE_NUM(x)
#define LL_PROFILE_ZONE_NAMED_CATEGORY_TEXTURE(x)
#define LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY(x)
#define STOP_GLERROR
#define LL_WARNS() std::cerr
#define LL_WARNS_ONCE() std::cerr
#define LL_ERRS() std::cerr
#define LL_INFOS() std::cerr
#define LL_ENDL std::endl
#define llassert_always(x) assert(x)
#define llassert(x) assert(x)
#define GL_FALSE 0
bool enabled=true; int gSavedSettings=0;
template<class T> struct LLCachedControl{
 LLCachedControl(int&,const char*,T){} operator T()const{return T(enabled);}
};
'''


def run_native(name, code, flags=(), mutant=False):
    result = compile_run(OUT, COMMON + code, name=name, flags=['/O2', '/WX', *flags])
    if mutant:
        assert result.returncode != 0, 'planted negative escaped: ' + name
        print('PASS rejected planted negative: ' + name)
    else:
        assert result.returncode == 0, result.stdout + result.stderr
        print(result.stdout.strip())


def budgets():
    region_path = 'indra/newview/llviewerregion.cpp'
    routine = function(read(region_path), 'LLViewerRegion::createVisibleObjects(')
    old = function(base(region_path), 'LLViewerRegion::createVisibleObjects(').replace(
        '::createVisibleObjects(', '::legacyCreateVisibleObjects(')
    display = read('indra/newview/llviewerdisplay.cpp')
    begin = display.index('            static LLCachedControl<bool> cap_geom_budget')
    end = display.index('            stop_glerror();', begin)
    geom = display[begin:end]
    code = r'''
float elapsed=0; std::vector<float> times; size_t created=0;bool persist_state=false;
struct LLViewerObject{} object;
struct LLTimer { float getElapsedTimeF32(){return elapsed;} };
struct LLVOCacheEntry{
 enum{WAITING=1}; int state=0; bool fails=false; int getState(){return state;}
 using vocache_entry_priority_list_t=std::vector<LLVOCacheEntry*>;
};
struct Partition{bool history=false;void setCullHistory(bool v){history=v;}};
struct LLViewerRegion{
 static int sNewObjectCreationThrottle;
 bool mDead=false; Partition partition;
 struct Impl{LLVOCacheEntry::vocache_entry_priority_list_t mWaitingList;Partition* mVOCachePartition;};
 Impl impl; Impl* mImpl=&impl;
 LLViewerRegion(){impl.mVOCachePartition=&partition;}
 LLViewerObject* addNewObject(LLVOCacheEntry* e){
  elapsed=times.at(created++);if(e->fails)return nullptr;
  if(persist_state)e->state=LLVOCacheEntry::WAITING;return &object;
 }
 void createVisibleObjects(float);void legacyCreateVisibleObjects(float);
};
int LLViewerRegion::sNewObjectCreationThrottle=0;
struct Interval{float n;float value(){return n;}} gFrameIntervalSeconds;
struct Pipeline{
 float create=0,geom=0;int partitions=0;
 void createObjects(float n){create=n;}void updateGeom(float n){geom=n;}
 void processPartitionQ(){++partitions;}
} gPipeline;
void display_geometry(){
''' + geom + '\n}\n' + routine + '\n' + old + r'''
int main(){
 for(bool on:{false,true}) for(float interval:{0.f,.016f,.1f,.5f,20.f,120.f}){
  enabled=on;gFrameIntervalSeconds.n=interval;display_geometry();
  float want=.005f*10.f*interval;if(on)want=std::min(want,.005f);
  assert(gPipeline.create==want && gPipeline.geom==want);
 }
 LLViewerRegion r;LLVOCacheEntry entries[8];
 for(auto& e:entries)r.impl.mWaitingList.push_back(&e);
 auto count=[&](bool on,int throttle,std::vector<float> clock,bool legacy=false){
  created=0;elapsed=0;enabled=on;times=clock;
  LLViewerRegion::sNewObjectCreationThrottle=throttle;
  if(legacy)r.legacyCreateVisibleObjects(.005f);else r.createVisibleObjects(.005f);
  return created;
 };
 const std::vector<float> crossing={.001f,.002f,.006f,.007f,.008f,.009f,.010f,.011f};
 assert(count(true,2,crossing)==2); // maximum count, even before deadline
 assert(count(true,6,crossing)==3); // deadline before maximum count
 assert(count(false,2,crossing)==8);
 const std::vector<float> expensive(8,.010f);
 assert(count(true,2,expensive)==1); // time limit before maximum count
 for(int throttle:{-2,-1,0,1,2,8,20})for(auto clock:{crossing,expensive}){
  const auto old_count=count(false,throttle,clock,true);
  assert(count(false,throttle,clock)==old_count);
  if(throttle<=0)assert(count(true,throttle,clock)==old_count);
 }
 entries[0].state=LLVOCacheEntry::WAITING;
 assert(count(true,2,crossing)==2); // already-waiting entries do not consume the count
 r.mDead=true;assert(count(true,2,crossing)==0);
 r.mDead=false;r.impl.mWaitingList.clear();assert(count(true,2,crossing)==0 && !r.partition.history);
 // Rebuild priority order each frame, as the viewer does. A permanently failing
 // head must not starve any of the seven healthy entries at count limit one.
 persist_state=true;for(auto& e:entries)e.state=0;entries[0].fails=true;
 for(int frame=0;frame<7;++frame){
  r.impl.mWaitingList.clear();
  for(auto& e:entries)if(e.state<LLVOCacheEntry::WAITING)r.impl.mWaitingList.push_back(&e);
  assert(count(true,1,std::vector<float>(8,.001f))==2);
  assert(entries[frame+1].state==LLVOCacheEntry::WAITING);
 }
 for(int i=1;i<8;++i)assert(entries[i].state==LLVOCacheEntry::WAITING);
 // Failure is free only against the count limit, never against elapsed time.
 entries[1].state=0;r.impl.mWaitingList={&entries[0],&entries[1]};
 assert(count(true,1,expensive)==1 && entries[1].state==0);
 std::cout<<"PASS geometry cap, count OR time limit, OFF oracle, sentinels, seven-frame failing-head progress and failure deadline\n";
}
'''
    run_native('budgets', code)
    run_native('budgets_no_cap', code.replace('llmin(legacy_geom_update_time, 0.005f)',
                                             'legacy_geom_update_time'), mutant=True)
    run_native('budgets_conjunction', code.replace('!(--throttle)) || update_timer',
                                                  '!(--throttle)) && update_timer'), mutant=True)
    run_native('budgets_failed_count', code.replace('objectp && !(--throttle)',
                                                   '(objectp || !objectp) && !(--throttle)'), mutant=True)


def uploads():
    path = 'indra/llrender/llimagegl.cpp'
    source = read(path)
    components = function(source, 'LLImageGL::dataFormatComponents(')
    typewidth = function(source, 'type_width_from_pixtype(')
    routine = function(source, 'void sub_image_lines(')
    old = function(base(path), 'void sub_image_lines(').replace('void sub_image_lines(', 'void legacy_lines(')
    stagger = function(source, 'bool should_stagger_image_set(')
    constants = sorted(set(re.findall(r'\bGL_\w+', components + typewidth + routine)))
    defines = '\n'.join(f'#define {key} {i+1}' for i, key in enumerate(constants))
    formats = [name for name in re.findall(r'case (GL_\w+):', components) if 'COMPRESSED' not in name]
    types = re.findall(r'case (GL_\w+):', typewidth)
    code = defines + r'''
#define LL_MESA_HEADLESS 0
#define LL_LINUX 0
#define LL_DARWIN 0
bool main_thread=true;bool on_main_thread(){return main_thread;}
struct {bool mIsIntel=false;}gGLManager;
struct LLImageGL{static int dataFormatComponents(int);};
struct LLImageGLThread{static bool sBatchedRowUploads;};
bool LLImageGLThread::sBatchedRowUploads=true;
// Observe the debug assertion while continuing to exercise release fallback.
int compression_asserts=0;
#undef llassert
#define llassert(x) do { if(!(x)) ++compression_asserts; } while(false)
''' + components + '\n' + typewidth + '\n' + stagger + r'''
int rowLength=0,alignment=1;size_t calls=0,queries=0;int dstWidth=0;
const U8* source_begin=nullptr;size_t source_size=0;
std::vector<U8> output;
size_t pixel_size(U32 fmt,U32 type){
 return type==GL_UNSIGNED_INT_8_8_8_8_REV?4:LLImageGL::dataFormatComponents(fmt)*type_width_from_pixtype(type);
}
void glGetIntegerv(U32 name,int* out){++queries;assert(main_thread);*out=name==GL_UNPACK_ROW_LENGTH?rowLength:alignment;}
void glTexSubImage2D(U32,int,int x,int y,int width,int height,U32 fmt,U32 type,const U8* src){
 ++calls;const size_t bpp=pixel_size(fmt,type);
 const size_t bytes=size_t(rowLength?rowLength:width)*bpp;
 const size_t stride=(bytes+alignment-1)/alignment*alignment;
 for(int row=0;row<height;++row){
  size_t dest=(size_t(y+row)*dstWidth+x)*bpp;
  assert(dest+width*bpp<=output.size());
  const size_t offset=size_t(src-source_begin)+row*stride;
  assert(offset<=source_size && size_t(width)*bpp<=source_size-offset);
  std::memcpy(output.data()+dest,src+row*stride,width*bpp);
 }
}
''' + routine + '\n' + old + r'''
int main(){
 const std::vector<U32> formats={FORMATS};const std::vector<U32> types={TYPES};
 size_t cases=0;
 for(U32 fmt:formats)for(U32 type:types){
  if(type==GL_UNSIGNED_INT_8_8_8_8_REV && fmt!=GL_RGBA && fmt!=GL_BGRA)continue;
  for(int width:{1,3,7,16,24,31,32,63,511,513,1024,1500,2048,17000})
  for(int height:{1,3,16,30,31,32,33,64,65})for(int extra:{0,1,7})
  for(int align:{1,2,4,8}){
   if(width>63 && !(fmt==GL_RGBA && type==GL_UNSIGNED_BYTE))continue;
   int dataWidth=width+extra;rowLength=extra?dataWidth:0;alignment=align;
   const size_t bpp=pixel_size(fmt,type),stride=(size_t(dataWidth)*bpp+align-1)/align*align;
   // Exact source allocation whenever batching is valid, including a narrow
   // rectangle ending on the final source row. Alignment fallback uses the
   // exact envelope of the legacy mixed padded/unpadded increments.
   const size_t lineBytes=size_t(dataWidth)*bpp;
   const int batch=width==dataWidth && height%32==0?(width>1024?8:width>512?16:32):1;
   const size_t legacySize=size_t(height-batch)*lineBytes+size_t(batch-1)*stride+size_t(width)*bpp;
   std::vector<U8> src(stride==lineBytes?size_t(height-1)*lineBytes+size_t(width)*bpp:legacySize);
   source_begin=src.data();source_size=src.size();
   for(size_t i=0;i<src.size();++i)src[i]=U8((i*29+i/7)%251);
   dstWidth=width+5;output.assign(size_t(dstWidth)*(height+4)*bpp,0xCD);
   calls=0;legacy_lines(0,0,2,3,width,height,fmt,type,src.data(),dataWidth);
   auto expected=output;const auto oldCalls=calls;
   for(bool on:{false,true}){
    std::fill(output.begin(),output.end(),0xCD);calls=0;queries=0;LLImageGLThread::sBatchedRowUploads=on;
    sub_image_lines(0,0,2,3,width,height,fmt,type,src.data(),dataWidth,false);
    assert(queries==(on?2u:0u)); // once per upload, never per row or slice
    assert(output==expected);assert(rowLength==(extra?dataWidth:0)&&alignment==align);
    if(!on || stride!=size_t(dataWidth)*bpp)assert(calls==oldCalls);
    if(on && stride==size_t(dataWidth)*bpp){
     const size_t rows=std::max(size_t(1),size_t(65536)/(size_t(width)*bpp));
     assert(calls==(height+rows-1)/rows);
    }
   }
   ++cases;
  }
 }
 // Zero dimensions/components and off-thread calls retain the legacy path.
 // No GL context queries are allowed for an off-thread row-helper call.
 U8 byte=0;source_begin=&byte;source_size=1;rowLength=0;alignment=1;dstWidth=1;
 output.assign(1,0);queries=0;
 sub_image_lines(0,0,0,0,0,1,GL_RGBA,GL_UNSIGNED_BYTE,&byte,0,false);
 assert(queries==0);
 sub_image_lines(0,0,0,0,1,1,0,GL_UNSIGNED_BYTE,&byte,1,false); // unknown format has zero components
 main_thread=false;queries=0;
 sub_image_lines(0,0,0,0,1,1,GL_RED,GL_UNSIGNED_BYTE,&byte,1,false);
 assert(queries==0);main_thread=true;
 U8 rgba[4]={1,2,3,4};source_begin=rgba;source_size=4;output.assign(4,0);queries=0;calls=0;
 sub_image_lines(0,0,0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba,1,true);
 assert(compression_asserts==1 && queries==0 && calls==1);
 assert(std::equal(output.begin(),output.end(),rgba));
 assert(!should_stagger_image_set(true)); // compressed uploads never enter row slicing
 gGLManager.mIsIntel=true;assert(!should_stagger_image_set(false));
 std::cout<<"PASS "<<cases<<" upload cases: every source read bounded, exact-sized batching allocations, legacy byte oracle, formats/types, packed/narrow/odd/mips, strides, alignment fallback, offsets, context guard, query count, compression bypass\n";
}
'''.replace('FORMATS', ','.join(formats)).replace('TYPES', ','.join(types))
    run_native('uploads', code)
    run_native('uploads_wrong_stride', code.replace('size_t(data_width) * pixel_bytes',
                                                   'size_t(width) * pixel_bytes'), mutant=True)
    run_native('uploads_ignore_alignment', code.replace('if (unpack_stride == line_bytes &&',
                                                        'if (true &&'), mutant=True)
    run_native('uploads_source_overread', code.replace(
        'width, slice, pixformat, pixtype, src);',
        'width, slice, pixformat, pixtype, src + 1);'), mutant=True)
    run_native('uploads_zero_pixel_division', code.replace('pixel_bytes > 0', 'true'), mutant=True)
    run_native('uploads_zero_width_division', code.replace('&& width > 0 && height > 0',
                                                         '&& height > 0'), mutant=True)
    run_native('uploads_compression_guard', code.replace('&& !compressed && on_main_thread()',
                                                       '&& on_main_thread()'), mutant=True)
    run_native('uploads_context_guard', code.replace('&& on_main_thread() && width > 0',
                                                   '&& width > 0'), mutant=True)


def matrices():
    source = read('indra/llrender/llrender.cpp')
    names = ['is_affine_modelview(', 'LLRender::invalidateMatrixInverse(',
             'LLRender::getCachedMatrixInverse(', 'LLRender::syncMatrices(',
             'LLRender::translatef(', 'LLRender::scalef(', 'LLRender::ortho(',
             'LLRender::rotatef(', 'LLRender::pushMatrix(', 'LLRender::popMatrix(',
             'LLRender::loadMatrix(', 'LLRender::multMatrix(', 'LLRender::loadIdentity(']
    routines = '\n'.join(function(source, name) for name in names)
    # Count production inversion calls; reference inversions remain untouched.
    routines = routines.replace('glm::inverse(', 'counted_inverse(').replace(
        'glm::affineInverse(', 'counted_affine_inverse(')
    # Use the exact production constructor's matrix initialization block.
    start = source.index('    mMatrixMode = LLRender::MM_MODELVIEW;')
    end = source.index('    mLightHash = 0;', start)
    initialization = source[start:end]
    code = r'''
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
int general_count=0,affine_count=0;
glm::mat4 counted_inverse(const glm::mat4& m){++general_count;return glm::inverse(m);}
glm::mat4 counted_affine_inverse(const glm::mat4& m){++affine_count;return glm::affineInverse(m);}
struct LLShaderMgr{enum{
 MODELVIEW_MATRIX,PROJECTION_MATRIX,TEXTURE_MATRIX0,TEXTURE_MATRIX1,TEXTURE_MATRIX2,TEXTURE_MATRIX3,
 NORMAL_MATRIX,INVERSE_MODELVIEW_MATRIX,MODELVIEW_PROJECTION_MATRIX,INVERSE_PROJECTION_MATRIX,IDENTITY_MATRIX
};};
struct LLGLSLShader{
 static LLGLSLShader* sCurBoundShaderPtr;
 struct Features{bool hasLighting=false,calculatesLighting=false,calculatesAtmospherics=false;}mFeatures;
 U32 mMatHash[6]={~0u,~0u,~0u,~0u,~0u,~0u};bool mvp=false;
 int mvWrites=0,projWrites=0;
 glm::mat4 mv{1},inv{1},proj{1},invproj{1},mvpValue{1};glm::mat3 normal{1};
 int getUniformLocation(int key){return key==LLShaderMgr::MODELVIEW_PROJECTION_MATRIX&&!mvp?-1:key;}
 void uniformMatrix4fv(int key,int,int,const float* v){
  if(key==LLShaderMgr::MODELVIEW_MATRIX){mv=glm::make_mat4(v);++mvWrites;}
  if(key==LLShaderMgr::PROJECTION_MATRIX){proj=glm::make_mat4(v);++projWrites;}
  if(key==LLShaderMgr::MODELVIEW_PROJECTION_MATRIX)mvpValue=glm::make_mat4(v);
  if(key==LLShaderMgr::INVERSE_MODELVIEW_MATRIX)inv=glm::make_mat4(v);
  if(key==LLShaderMgr::INVERSE_PROJECTION_MATRIX)invproj=glm::make_mat4(v);
 }
 void uniformMatrix3fv(int,int,int,const float* v){normal=glm::make_mat3(v);}
};
LLGLSLShader* LLGLSLShader::sCurBoundShaderPtr=nullptr;
constexpr U32 LL_MATRIX_STACK_DEPTH=16;
struct LLRender{
 enum eMatrixMode:U8{MM_MODELVIEW,MM_PROJECTION,MM_TEXTURE0,MM_TEXTURE1,MM_TEXTURE2,MM_TEXTURE3,NUM_MATRIX_MODES};
 static bool sCachedMatrixInverse;
 eMatrixMode mMatrixMode=MM_MODELVIEW;U32 mMatIdx[6]{},mMatHash[6]{},mCurMatHash[6]{};
 glm::mat4 mMatrix[6][LL_MATRIX_STACK_DEPTH],mCachedInverse[2];
 bool mCachedInverseValid[2]={false,false},mCachedInverseEnabled=true;
 LLRender(){INITIALIZATION}
 void flush(){}void syncLightState(){}void syncMatrices();void invalidateMatrixInverse(eMatrixMode);
 const glm::mat4& getCachedMatrixInverse(eMatrixMode);
 void translatef(const GLfloat&,const GLfloat&,const GLfloat&);
 void scalef(const GLfloat&,const GLfloat&,const GLfloat&);
 void rotatef(const GLfloat&,const GLfloat&,const GLfloat&,const GLfloat&);
 void ortho(F32,F32,F32,F32,F32,F32);
 void pushMatrix();void popMatrix();void loadMatrix(const GLfloat*);void multMatrix(const GLfloat*);void loadIdentity();
};
bool LLRender::sCachedMatrixInverse=true;
'''.replace('INITIALIZATION', initialization) + routines + r'''
bool close(const glm::mat4& a,const glm::mat4& b){
 for(int c=0;c<4;++c)for(int r=0;r<4;++r){
  if(!std::isfinite(a[c][r]) || std::abs(a[c][r]-b[c][r])>2e-4f*(1+std::abs(b[c][r])))return false;
 }return true;
}
int main(){
 LLRender r;int syncs=0;
 auto check=[&](){
  LLGLSLShader shader;LLGLSLShader::sCurBoundShaderPtr=&shader;r.syncMatrices();++syncs;
  auto expected=glm::inverse(r.mMatrix[0][r.mMatIdx[0]]);
  assert(close(shader.inv,expected));
  assert(close(shader.invproj,glm::inverse(r.mMatrix[1][r.mMatIdx[1]])));
  auto norm=glm::mat3(glm::transpose(expected));
  for(int c=0;c<3;++c)for(int row=0;row<3;++row)assert(std::abs(norm[c][row]-shader.normal[c][row])<2e-4f);
 };
 check();int inverse_calls=general_count+affine_count;
 for(int i=0;i<20;++i)check();assert(general_count+affine_count==inverse_calls);
 for(auto mode:{LLRender::MM_MODELVIEW,LLRender::MM_PROJECTION}){
  r.mMatrixMode=mode;r.loadIdentity();check();
  r.translatef(3,4,5);check();r.scalef(2,3,.5);check();r.rotatef(31,1,2,3);check();
  r.ortho(-4,5,-3,7,.1f,90);check();r.pushMatrix();assert(!r.mCachedInverseValid[mode]);check();
  r.translatef(9,8,7);check();r.pushMatrix();r.scalef(3,4,5);check();
  r.popMatrix();check();r.popMatrix();check();
  glm::mat4 custom=glm::translate(glm::mat4(1),glm::vec3(-7,2,3));
  r.loadMatrix(glm::value_ptr(custom));check();
  custom=glm::rotate(custom,.3f,glm::vec3(0,1,0));r.multMatrix(glm::value_ptr(custom));check();
  r.loadIdentity();check();
 }
 glm::mat4 projective=glm::perspective(.9f,1.3f,.1f,100.f);
 r.mMatrixMode=LLRender::MM_PROJECTION;r.loadMatrix(glm::value_ptr(projective));
 int general_before=general_count;check();assert(general_count==general_before+1);
 assert(!is_affine_modelview(projective));
 r.mMatrixMode=LLRender::MM_MODELVIEW;r.loadMatrix(glm::value_ptr(projective));
 int before=affine_count;check();assert(before==affine_count);
 for(int col=0;col<4;++col){glm::mat4 tiny(1);tiny[col][3]+=col==3?.001f:1e-12f;assert(!is_affine_modelview(tiny));}
 glm::mat4 mirror(1);mirror[0][0]=-1;assert(is_affine_modelview(mirror));
 r.loadMatrix(glm::value_ptr(mirror));check();
 // Switch OFF and back without a matrix write, then through a real write.
 for(int j=0;j<4;++j){LLRender::sCachedMatrixInverse=false;check();r.translatef(1,2,3);check();
  LLRender::sCachedMatrixInverse=true;check();}
 LLGLSLShader shader;shader.mvp=true;LLGLSLShader::sCurBoundShaderPtr=&shader;r.syncMatrices();
 for(bool on:{false,true,false,true}){
  int mvBefore=shader.mvWrites,projBefore=shader.projWrites;
  LLRender::sCachedMatrixInverse=on;r.syncMatrices();
  assert(shader.mvWrites==mvBefore+1 && shader.projWrites==projBefore+1);
  auto expected=glm::inverse(r.mMatrix[0][r.mMatIdx[0]]);assert(close(shader.inv,expected));
  assert(close(shader.invproj,glm::inverse(r.mMatrix[1][r.mMatIdx[1]])));
  assert(close(shader.mvpValue,r.mMatrix[1][r.mMatIdx[1]]*r.mMatrix[0][r.mMatIdx[0]]));
  auto norm=glm::mat3(glm::transpose(expected));
  for(int c=0;c<3;++c)for(int row=0;row<3;++row)assert(std::abs(norm[c][row]-shader.normal[c][row])<2e-4f);
 }
 // Separate LLRender instances must never share an inverse cache.
 LLRender other;other.translatef(-90,12,4);LLGLSLShader::sCurBoundShaderPtr=&shader;other.syncMatrices();
 assert(close(shader.inv,glm::inverse(other.mMatrix[0][0])));
 std::cout<<"PASS "<<syncs<<" matrix syncs: real GLM, every stack writer, MVP-less reuse, projective fallback, projection general, toggles, instances\n";
}
'''
    cache = __import__('json').loads(Path('C:/Users/xianw/cxs/zs_flags_cache.json').read_text(encoding='utf-8-sig'))
    # GLM is header-only; use the already installed dependency, with no dependency writes.
    inc = next(p for item in cache.values() for flag in item['flags']
               for p in re.findall(r'"([^"]+)"', flag)
               if (Path(p) / 'glm/glm.hpp').is_file())
    flags = [f'/I"{inc}"']
    run_native('matrices', code, flags)
    # Each missing writer invalidation must be caught by native output comparison.
    for writer in ['translatef', 'scalef', 'ortho', 'rotatef', 'pushMatrix', 'popMatrix', 'loadMatrix', 'multMatrix', 'loadIdentity']:
        body = function(code, 'LLRender::' + writer + '(')
        mutant = body.replace('invalidateMatrixInverse(mMatrixMode);', '')
        assert body != mutant
        run_native('matrices_missing_' + writer, code.replace(body, mutant), flags, mutant=True)
    run_native('matrices_projective_affine', code.replace(
        'mode == MM_MODELVIEW && is_affine_modelview(mat)', 'mode == MM_MODELVIEW'), flags, mutant=True)
    run_native('matrices_projection_affine', code.replace(
        'mode == MM_MODELVIEW && is_affine_modelview(mat)', 'true'), flags, mutant=True)
    run_native('matrices_stale_toggle', code.replace(
        '++mMatHash[MM_MODELVIEW];', '').replace('++mMatHash[MM_PROJECTION];', ''), flags, mutant=True)


def contracts():
    root = ET.fromstring(read('indra/newview/app_settings/settings.xml')).find('map')
    entries = list(root)
    settings = {k.text:v for k,v in zip(entries[::2],entries[1::2])}
    for suffix in ['GeomBudgetCap','BatchedRowUploads','CachedMatrixInverse']:
        name = 'AlchemyPerf'+suffix
        fields = list(settings[name]); meta = {k.text:v.text for k,v in zip(fields[::2],fields[1::2])}
        assert (meta['Persist'],meta['Type'],meta['Value']) == ('1','Boolean','1')
        assert 'FALSE restores the exact previous behaviour' in meta['Comment']
        assert 'LLCachedControl<bool>' in read('indra/newview/llviewercontrol.cpp')
    # Enumerate all stack writes. An added writer must invalidate or fail this fixture.
    render = read('indra/llrender/llrender.cpp')
    writer_lines = [line.strip() for line in render.splitlines() if re.search(r'mMatrix\[.*(?:=|\*=)', line)
                    and not line.strip().startswith('//')]
    assert len(writer_lines) == 9, writer_lines
    assert render.count('invalidateMatrixInverse(mMatrixMode);') == 9
    assert 'invalidateMatrixInverse(eMatrixMode(i));' in render
    print('PASS switch metadata and matrix writer census (constructor + 8 writes + pop)')


if __name__ == '__main__':
    OUT.mkdir(parents=True, exist_ok=True)
    contracts()
    budgets()
    uploads()
    matrices()

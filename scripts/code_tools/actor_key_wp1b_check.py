"""WP1b production avatar-pool/suppression/capture/coverage methods and WGL numeric fixtures.

Compiles a test DLL against existing core libraries; never builds/launches the viewer.
Synthetic scene objects supply geometry/material/readiness only. The complete production
avatar-pool method, suppression predicates, system beauty/native coverage helpers and
capture/replay implementation execute. Full production shaders provide numeric coverage.
"""
from pathlib import Path
import ctypes as C, os, subprocess, re, json
from native_check_tools import compile_run, viewer_native_flags
from relight_area_policy_check import method
ROOT=Path(__file__).resolve().parents[2];NV=ROOT/'indra/newview';OUT=ROOT/'.tmp/ckey_20261102/wp1b'

def native_source():
    prefix=Path(__file__).with_name('actor_key_wp1b_fixture.h').read_text(encoding='utf-8')
    prefix=prefix.replace('// GP_INSERT',method(NV/'llactormover.h','struct GhostDrawParams')+';')
    prefix=prefix.replace('#include "alactorkeycapture.h"','#define private public\n#include "alactorkeycapture.h"\n#undef private')
    prefix=prefix.replace('struct FakeDrawable{};','struct FakeDrawable:LLRefCount{};').replace('mDrawable=reinterpret_cast<FakeDrawable*>(1);','mDrawable=new FakeDrawable;').replace('inline static void*sWhiteImagep','inline static LLTexture*sWhiteImagep')
    prefix+=method(NV/'llactormaskeligibility.h','class LLActorMaskTextureState')+';\n'
    for name in ('LLActorMaskTextureState::LLActorMaskTextureState(','void LLActorMaskTextureState::restore('):prefix+=method(NV/'llactormaskeligibility.cpp',name)+'\n'
    source=(NV/'alactorchromakey.cpp').read_text(encoding='utf-8')
    capture=source[source.index('namespace\n{',source.index('// [ActorKey][WP1b]')):source.index('bool ALActorKeyCapture::beautyContext()')]
    prefix+=capture+'\nbool ALActorKeyCapture::beautyContext(){return ALActorChromaKey::sActive&&!ALActorChromaKey::blocked;}\n'
    prefix+=method(NV/'llactormover.cpp','enum EGhostStyle')+';\n'
    for name in ('bool shared_actor_fx_zero_cover(\n','bool shared_actor_fx_style_suppresses(','bool LLRenderPass::shouldSuppressSharedActorFx(const LLUUID&','bool LLRenderPass::shouldSuppressSharedActorFx(const LLDrawInfo&','bool LLRenderPass::shouldSuppressSharedActorFxPBR('):prefix+=method(NV/'lldrawpool.cpp',name)+'\n'
    prefix+=method(NV/'lldrawpoolavatar.cpp','static LLUUID get_actor_fx_id(')+'\n'+method(NV/'lldrawpoolavatar.cpp','LLUUID ALActorKeyCapture::systemActorId(')+'\n'
    prefix+=method(NV/'lldrawpoolavatar.cpp','void LLDrawPoolAvatar::renderAvatars(')+'\n'
    system=method(NV/'llactormover.cpp','bool LLActorMover::renderSystemActorGhost(')
    geometry=method(NV/'llactormover.cpp','S32 drawGeometryGhost(')
    mover=(NV/'llactormover.cpp').read_text(encoding='utf-8')
    for name in sorted(set(re.findall(r'\bsGhost\w+',system+geometry))):
        match=re.search(r'\b'+name+r'\s*\(\s*"([^"]+)"\s*\)',mover);assert match,name
        prefix+='static LLStaticHashedString '+name+'("'+match[1]+'");\n'
    baseline=OUT/'baseline_mover.cpp';baseline.write_bytes(subprocess.check_output(['git','show','HEAD:indra/newview/llactormover.cpp']))
    prefix+=method(baseline,'U32 LLActorMover::renderSystemSilCover(').replace('LLActorMover::renderSystemSilCover(', 'LLActorMover::renderSystemSilCoverBaseline(')+'\n'
    prefix+=geometry+'\n'
    prefix+=system+'\n'+method(NV/'llactormover.cpp','U32 LLActorMover::renderSystemSilCover(')+'\n'
    style_upload=method(NV/'lldrawpool.cpp','bool upload_actor_fx_style(')
    pool=(NV/'lldrawpool.cpp').read_text(encoding='utf-8')
    for name in sorted(set(re.findall(r'\bsActorFx\w+',style_upload))):
        match=re.search(r'\b'+name+r'\s*\(\s*"([^"]+)"\s*\)',pool);assert match,name
        prefix+='static LLStaticHashedString '+name+'("'+match[1]+'");\n'
    prefix+=style_upload+'\nvoid LLRenderPass::uploadActorFxStyleOnly(LLUUID id,bool wire,bool){upload_actor_fx_style(id,LLDirectorCast::instance().style,wire); }\n'
    prefix+=method(NV/'lldrawpool.cpp','void LLRenderPass::pushGLTFBatchIndexed(')+'\n'+method(NV/'llactormover.cpp','bool LLActorMover::renderSharedActorPBRCommands(')+'\n'
    prefix+=method(NV/'pipeline.cpp','LLRender::eBlendFactor silCoverBlendFactor(')+'\n'+method(NV/'pipeline.cpp','class ScopedSilCoverState')+';\n'
    prefix+=method(NV/'pipeline.cpp','bool LLPipeline::renderCover(')+'\n'+method(NV/'pipeline.cpp','void LLPipeline::buildActorKeyCoverageAndMatte()')+'\n'
    prefix+=r'''
static LLPointer<LLVOAvatar> sourceAvatar;
static LLDrawInfo sceneInfo[2];static LLDrawInfo*activeInfo=&sceneInfo[0];
#define fixtureInfo (*activeInfo)
static LLGLSLShader meshBeauty;
static LLDrawPoolAvatar avatarPool;
static void install(LLGLSLShader& sh,GLuint p){sh.mProgramObject=p;sh.mapAttributes();sh.mapUniforms();sh.mShaderHash=LLUUID("22222222-2222-2222-2222-222222222222");}
extern "C" {
__declspec(dllexport) int init(){
 LLCommon::initClass();gGLManager.initGL();gGL.init(false);fixtureShaderMgr.initAttribsAndUniforms();for(int i=0;i<16;++i)gGLProjection[i]=gGLModelView[i]=(i%5==0?1.f:0.f);LLVertexBuffer::initClass(nullptr);
 fixtureBuffer=new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX|LLVertexBuffer::MAP_NORMAL|LLVertexBuffer::MAP_TEXCOORD0|LLVertexBuffer::MAP_COLOR|LLVertexBuffer::MAP_WEIGHT|LLVertexBuffer::MAP_WEIGHT4|LLVertexBuffer::MAP_TANGENT|LLVertexBuffer::MAP_TEXTURE_INDEX);
 fixtureBuffer->allocateBuffer(3,3);LLStrider<LLVector4a> v,tan;LLStrider<LLVector3>n;LLStrider<LLVector2> uv;LLStrider<LLColor4U> c;LLStrider<U16> ix;LLStrider<F32>w;LLStrider<LLVector4>w4;
 fixtureBuffer->getWeightStrider(w);fixtureBuffer->getWeight4Strider(w4);fixtureBuffer->getTangentStrider(tan);fixtureBuffer->getVertexStrider(v);fixtureBuffer->getNormalStrider(n);fixtureBuffer->getTexCoord0Strider(uv);fixtureBuffer->getColorStrider(c);fixtureBuffer->getIndexStrider(ix);
 for(int i=0;i<3;++i){v[i].set(i==1?3.f:-1.f,i==2?3.f:-1.f,0,0);tan[i].set(1,0,0,1);n[i]=LLVector3(0,0,1);uv[i]=LLVector2(i==1?2.f:0.f,i==2?2.f:0.f);c[i]=LLColor4U(255,255,255,255);ix[i]=i;w[i]=0;w4[i]=LLVector4(.25,.25,.25,.25);}
 fixtureBuffer->unmapBuffer();sceneInfo[0].ref();sceneInfo[1].ref();LLGLSLShader::sSharedPBRIndexedGLTFChannels=2;LLGLSLShader::sIndexedTextureChannels=4;return 1;
}
__declspec(dllexport) void programs(unsigned body,unsigned eye,unsigned nativeBody,unsigned nativeEye,unsigned bodyCov,unsigned eyeCov,unsigned mesh,unsigned meshCov){
 for(auto& sh:gSilCoverProgram)install(sh,nativeBody);
 install(gAvatarActorGhostProgram,body);install(gAvatarEyeballActorGhostProgram,eye);install(nativeShader,nativeBody);install(gSilCoverProgram[ACTOR_MASK_BODY],nativeBody);install(gSilCoverProgram[ACTOR_MASK_EYES],nativeEye);install(meshBeauty,mesh);
 for(auto pair:{std::make_pair(&gAvatarActorGhostProgram,bodyCov),std::make_pair(&gAvatarEyeballActorGhostProgram,eyeCov),std::make_pair(&meshBeauty,meshCov)}){
 auto& kp=sKeyPrograms[pair.first];if(!kp)kp=std::make_unique<KeyProgram>();install(kp->shader,pair.second);kp->hash=pair.first->mShaderHash;}
}
__declspec(dllexport) void begin(int mode,int look,float alpha,int ready,int pbrReady,int hidden,int self,unsigned texture){
 ++gFrameCount;activeInfo=&sceneInfo[0];ALActorKeyCapture::instance().beginBeauty();fixtureTexture=texture;
 if(fixtureDiffuse.notNull())fixtureDiffuse->image->setTexName(0);fixtureDiffuse=new LLViewerTexture;fixtureDiffuse->external(texture);
 sourceAvatar=new LLVOAvatar;sourceAvatar->hidden=hidden==1;ALDerenderList::hidden=hidden==2;
 sourceAvatar->id=ALActorChromaKey::selected;gAgentID=self?sourceAvatar->id:LLUUID("33333333-3333-3333-3333-333333333333");
 auto& style=LLDirectorCast::instance().style;style.mMode=mode;style.mStyle=look;style.mAlpha=alpha;style.mDistortion=0;style.mDistortionAmount=0;
 LLActorMover::instance().ready=ready;LLActorMover::instance().pbrReady=pbrReady;
 sceneInfo[0].mActorFxOwner=sceneInfo[1].mActorFxOwner=sourceAvatar->id;nativeBodyDraws=nativeEyeDraws=sharedDraws=0;
}
__declspec(dllexport) void beauty(int part){
 fixturePart=part;
 LLDrawPoolAvatar::sVertexProgram=&nativeShader;nativeShader.bind();LLDrawPoolAvatar::sSkipOpaque=part==1;LLDrawPoolAvatar::sSkipTransparent=part!=1;
 avatarPool.renderAvatars(sourceAvatar,part==2?1:2);
 // The actual shared beauty helper captures its body/eyes/transparent emissions.
 if(!sourceAvatar->hidden&&!ALDerenderList::hidden&&LLActorMover::instance().ready&&LLDirectorCast::instance().style.mAlpha>0){
 LLActorMover::instance().renderSystemActorGhost(sourceAvatar,sourceAvatar->isSelf()?LLUUID::null:sourceAvatar->id,part==1,false,true);}
}

__declspec(dllexport) void pbr_program(unsigned p,unsigned cov,int variant){
 LLGLSLShader* sh=variant==0?&gSharedActorFxPBRProgram[2]:variant==1?&gSharedActorFxSkinnedPBRProgram[2]:&gSharedActorFxSkinnedPBRSlotProgram[2];sh->mFeatures.mIndexedMaterialChannels=variant==2?2:0;install(*sh,p);
 auto& kp=sKeyPrograms[sh];if(!kp)kp=std::make_unique<KeyProgram>();kp->shader.mFeatures=sh->mFeatures;install(kp->shader,cov);kp->hash=sh->mShaderHash;
}
__declspec(dllexport) void mesh_beauty(int pbr){
 meshBeauty.bind();fixtureInfo.mAvatar=sourceAvatar;fixtureInfo.mVertexBuffer=fixtureBuffer;
 bool suppressed=pbr?LLRenderPass::shouldSuppressSharedActorFxPBR(fixtureInfo):LLRenderPass::shouldSuppressSharedActorFx(fixtureInfo);
 if(!sourceAvatar->hidden&&!ALDerenderList::hidden&&LLDirectorCast::instance().style.mAlpha>0&&(pbr?LLActorMover::instance().pbrReady:LLActorMover::instance().ready)){
 auto& st=LLDirectorCast::instance().style;
 if(pbr){
 LLPointer<LLViewerTexture> t=new LLViewerTexture;t->external(fixtureTexture);LLPointer<LLFetchedGLTFMaterial>mat=new LLFetchedGLTFMaterial;mat->mBaseColorTexture=t;mat->mNormalTexture=t;mat->mMetallicRoughnessTexture=t;mat->mEmissiveTexture=t;mat->mDoubleSided=true;fixtureInfo.mGLTFMaterial=mat;fixtureInfo.mGLTFMaterialList={mat,mat};
 LLActorMover::SharedActorStyleProxy proxy;proxy.mAvatar=sourceAvatar;proxy.mStyleId=sourceAvatar->isSelf()?LLUUID::null:sourceAvatar->id;proxy.mAlpha=st.mAlpha;proxy.mSuppressible=st.mMode==LLDirectorCast::ACTOR_STYLE_REPLACE;proxy.mStyle=st.mStyle;
 LLFace face;face.mDrawInfo=&fixtureInfo;LLActorMover::SharedActorPBRCommand cmd;cmd.mMaterial=mat;cmd.mRigged=pbr!=1;cmd.mIndexed=pbr==3;if(cmd.mRigged)cmd.mBatch.mInfo=&fixtureInfo;else cmd.mStaticFace.mFace=&face;proxy.mPBRCommands.push_back(cmd);
 LLActorMover::instance().renderSharedActorPBRCommands(proxy,false);
 t->image->setTexName(0); // fixture borrows Python's texture; never deletes it
 }else if(suppressed){
 LLDrawPoolAvatar::sVertexProgram=&meshBeauty;fixtureDraw();bool bearing=st.mMode==LLDirectorCast::ACTOR_STYLE_REPLACE||st.mStyle==10;
 ALActorKeyCapture::instance().capture(&fixtureInfo,fixtureInfo.mActorFxOwner,bearing,fixtureBuffer,0,2,3,0,false,true);}
 }
}

__declspec(dllexport) void legacy_program(unsigned p,unsigned cov,int indexed){
 auto*sh=indexed?&gWorldSkinnedActorGhostIndexedProgram:&gWorldActorGhostProgram;install(*sh,p);auto& kp=sKeyPrograms[sh];if(!kp)kp=std::make_unique<KeyProgram>();install(kp->shader,cov);kp->hash=sh->mShaderHash;
 gWorldActorGhostProgram.mRiggedVariant=&meshBeauty;
}
__declspec(dllexport) int legacy_beauty(int variant,int blend){
 meshBeauty.bind();fixtureInfo.mAvatar=sourceAvatar;fixtureInfo.mVertexBuffer=fixtureBuffer;
 fixtureInfo.mSkinInfo=new FakeDrawable;
 fixtureInfo.mGLTFMaterial=nullptr;fixtureInfo.mGLTFMaterialList.clear();fixtureInfo.mTextureList=variant==2?std::vector<LLPointer<LLViewerTexture>>{fixtureDiffuse,fixtureDiffuse}:std::vector<LLPointer<LLViewerTexture>>{};
 LLRenderPass::shouldSuppressSharedActorFx(fixtureInfo);
 auto& st=LLDirectorCast::instance().style;
 if(!sourceAvatar->hidden&&!ALDerenderList::hidden&&st.mAlpha>0&&LLActorMover::instance().ready){
 LLActorMover::GhostDrawParams gp;gp.mWorldLinear=true;gp.mCoverMode=st.mMode==LLDirectorCast::ACTOR_STYLE_REPLACE;gp.mCoverageLayerStrength=st.mMode==LLDirectorCast::ACTOR_STYLE_LAYER&&st.mStyle==10?st.mAlpha:-1;gp.mPhase=(F32)(sourceAvatar->id.mData[0]|(sourceAvatar->id.mData[1]<<8))*(F_TWO_PI/65536.f);gp.mNativeDepthAvailable=true;gp.mDistort=st.mDistortion;gp.mDistortAmount=st.mDistortionAmount;
 LLFace face;face.mDrawInfo=&fixtureInfo;std::vector<LLActorMover::GhostBatch>batches;std::vector<LLActorMover::GhostStaticFace>faces;
 if(variant==0)faces.push_back({&face,blend?2:0,.2,false,{}});else batches.push_back({&fixtureInfo,blend?1:0});
 bool complete=false;drawGeometryGhost(sourceAvatar,batches,{},LLColor4(1,1,1,1),st.mAlpha,st.mStyle,gp,&faces,&complete);return complete;
 }return 1;
}

__declspec(dllexport) void reverse_winding(int reverse){LLStrider<U16> ix;fixtureBuffer->getIndexStrider(ix);ix[0]=0;ix[1]=reverse?2:1;ix[2]=reverse?1:2;fixtureBuffer->unmapBuffer();}
__declspec(dllexport) void poison_inputs(){gAvatarActorGhostProgram.bind();glUniform4f(glGetUniformLocation(gAvatarActorGhostProgram.mProgramObject,"ghostSystemColor"),0,0,0,0);float zeros[180]={};glUniform4fv(glGetUniformLocation(gAvatarActorGhostProgram.mProgramObject,"matrixPalette[0]"),45,zeros);glUniform1f(glGetUniformLocation(gAvatarActorGhostProgram.mProgramObject,"ghostTime"),999);gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE,0);}
__declspec(dllexport) void distortion(){LLDirectorCast::instance().style.mDistortion=3;LLDirectorCast::instance().style.mDistortionAmount=.5;}
__declspec(dllexport) void mixed_beauty(int variant){activeInfo=&sceneInfo[0];mesh_beauty(variant+1);activeInfo=&sceneInfo[1];legacy_beauty(1,1);activeInfo=&sceneInfo[0];}
__declspec(dllexport) int off_baseline(){for(auto& x:sKeyPrograms)x.second->shader.mProgramObject=0;ALActorChromaKey::sActive=false;ALActorKeyCapture::instance().release();std::unordered_set<const LLDrawInfo*>fresh;return !ALActorKeyCapture::sArmed&&sKeyCommands.capacity()==0&&ALActorKeyCapture::instance().mBeautySuppressed.bucket_count()==fresh.bucket_count();}
__declspec(dllexport) int failed_k1(){gPipeline.failEnsure=true;gPipeline.buildActorKeyCoverageAndMatte();gPipeline.failEnsure=false;return !ALActorKeyCapture::sArmed&&gPipeline.mActorKeyCoverageFrame!=gFrameCount;}
__declspec(dllexport) int absent_capture(){return sKeyCommands.empty();}
__declspec(dllexport) int captured_nonbearing(){if(sKeyCommands.empty())return 0;for(const auto& c:sKeyCommands)if(c.bearing||!c.beauty||!c.draw||c.uniforms.empty())return 0;return 1;}
__declspec(dllexport) void blocked(int b){ALActorChromaKey::blocked=b;}
__declspec(dllexport) int coverage(int part,int mesh,int includeAlpha){
 fixtureIncludeAlpha=includeAlpha;gPipeline.mPools.clear();gPipeline.groups.clear();gPipeline.riggedGroups.clear();
 static LLFace face;static LLSpatialGroup group;
 if(mesh){group.mDrawMap[LLRenderPass::PASS_ALPHA_RIGGED]=mesh==2?std::vector<LLPointer<LLDrawInfo>>{&sceneInfo[0],&sceneInfo[1]}:std::vector<LLPointer<LLDrawInfo>>{&fixtureInfo};gPipeline.riggedGroups={&group};}
 if(!mesh||mesh==3){face.drawable.p=sourceAvatar;avatarPool.mDrawFace={&face};gPipeline.mPools={&avatarPool};}
 gPipeline.buildActorKeyCoverageAndMatte();gPipeline.mActorKeyCoverage.bindTarget();return gPipeline.mActorKeyCoverageFrame==gFrameCount?1:0;
}
__declspec(dllexport) void silhouette(int phase,int baseline){LLGLDepthTest depth(GL_FALSE,GL_FALSE);LLGLDisable blend(GL_BLEND);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);if(baseline)LLActorMover::instance().renderSystemSilCoverBaseline(sourceAvatar,LLVector2(.3,.7),phase);else LLActorMover::instance().renderSystemSilCover(sourceAvatar,LLVector2(.3,.7),phase,false);}
__declspec(dllexport) int count(int part,int mesh){auto& cap=ALActorKeyCapture::instance();if(mesh){auto t=cap.mDrawTrace[mesh==2?&sceneInfo[part]:&fixtureInfo];return t.native+t.shared;}auto t=cap.mSystemTrace[{sourceAvatar->id,static_cast<SystemComponentKey::Part>(part)}];return t.native+t.shared;}
__declspec(dllexport) int contributor(int part,int mesh){auto& cap=ALActorKeyCapture::instance();if(mesh)return cap.mDrawTrace[mesh==2?&sceneInfo[part]:&fixtureInfo].shared;return cap.mSystemTrace[{sourceAvatar->id,static_cast<SystemComponentKey::Part>(part)}].shared;}
__declspec(dllexport) void cleanup(){gPipeline.mActorKeyCoverage.release();sourceAvatar=nullptr;fixtureBuffer=nullptr;for(auto& x:sKeyPrograms)x.second->shader.mProgramObject=0;sKeyPrograms.clear();for(auto* p:{&nativeShader,&gAvatarActorGhostProgram,&gAvatarEyeballActorGhostProgram,&meshBeauty})p->mProgramObject=0;for(auto& p:gSilCoverProgram)p.mProgramObject=0;}
}
'''
    mutation=os.environ.get('ACTOR_KEY_WP1B_MUTATION','')
    if mutation=='native-leak':
        needle='if (mBeautySuppressed.count(info)) return false;';assert prefix.count(needle)==1;prefix=prefix.replace(needle,'if (false) return false;',1)
    if mutation=='system-leak':
        needle='if (mSystemSuppressed.count(key)) return false;';assert prefix.count(needle)==1;prefix=prefix.replace(needle,'if (false) return false;',1)
    return prefix

def build_native():
    OUT.mkdir(parents=True,exist_ok=True)
    flags,libs=viewer_native_flags();flags=[f for f in flags if 'TRACY_ENABLE' not in f]+['/LD','/I"'+str(NV)+'"']
    build=Path(subprocess.check_output(['git','rev-parse','--git-common-dir'],cwd=ROOT,text=True).strip()).resolve().parent/'build-Windows-vs2026-os'
    libs=[f'"{p}"' for p in build.glob('*/Release/*.lib') if p.parent.parent.name==p.stem]+[f'"{p}"' for p in build.glob('vcpkg_installed/*/lib/*.lib') if p.open('rb').read(8)==b'!<arch>\n']
    libs+=['/link','/OPT:REF','/FORCE:MULTIPLE','opengl32.lib','gdi32.lib','user32.lib','advapi32.lib','bcrypt.lib','shell32.lib','ws2_32.lib','dbghelp.lib','dbgeng.lib','winmm.lib','ole32.lib','version.lib','psapi.lib','synchronization.lib','iphlpapi.lib','rpcrt4.lib','imm32.lib','comdlg32.lib','oleaut32.lib','uuid.lib','dinput8.lib','dxguid.lib','shcore.lib','dwmapi.lib','wbemuuid.lib','dxgi.lib']
    binary=compile_run(OUT,native_source(),'wp1b_native_'+str(os.getpid()),flags,libs,False)
    return binary,build




def main():
    import numpy as np
    import relight_3d_gl as G
    from clone_looks_gl_check import Runner, SH, V
    binary,build=build_native()
    runner=Runner(64,64);g=runner.gl;print('GL:',g.renderer,flush=True)
    directories=[os.add_dll_directory(str(p)) for p in [build/'sharedlibs/Release',build/'newview/Release',*build.glob('vcpkg_installed/*/bin')] if p.exists()]
    dll=C.CDLL(str(binary));assert dll.init()==1
    dll.programs.argtypes=[C.c_uint]*8;dll.begin.argtypes=[C.c_int,C.c_int,C.c_float,C.c_int,C.c_int,C.c_int,C.c_int,C.c_uint]
    dll.beauty.argtypes=[C.c_int];dll.coverage.argtypes=[C.c_int,C.c_int,C.c_int];dll.count.argtypes=[C.c_int,C.c_int];dll.contributor.argtypes=[C.c_int,C.c_int]
    tree=G.SourceTree('WORKTREE');world=('GHOST_WORLD_PASS','GHOST_SHARED_DISSOLVE');sysdefs=world+('GHOST_SYSTEM_AVATAR',)
    body=runner.program(tree,sysdefs,SH+'avatar/avatarActorGhostV.glsl');eye=runner.program(tree,sysdefs,SH+'avatar/eyeballActorGhostV.glsl')
    bc=runner.program(tree,sysdefs+('AL_KEY_COVERAGE_OUT',),SH+'avatar/avatarActorGhostV.glsl');ec=runner.program(tree,sysdefs+('AL_KEY_COVERAGE_OUT',),SH+'avatar/eyeballActorGhostV.glsl')
    mesh=runner.program(tree,world+('HAS_SKIN',),V);mc=runner.program(tree,world+('HAS_SKIN','AL_KEY_COVERAGE_OUT'),V)
    def link(vs,fs,defs=''):
        pr=g.program()
        for text,kind in [(t,0x8B31) for t in (vs if isinstance(vs,list) else [vs])]+[(fs,0x8B30)]:
            sh=g.create_shader(kind);src=C.c_char_p(('#version 420\n'+G.PREAMBLE+defs+text.replace('/*[EXTRA_CODE_HERE]*/','')).encode());g.source(sh,1,C.byref(src),None);g.compile(sh);ok=g.I();g.getshader(sh,0x8B81,C.byref(ok))
            if not ok.value:
                log=C.create_string_buffer(64000);g.slog(sh,len(log),None,log);raise AssertionError(log.value.decode())
            g.attach(pr,sh)
        g.link(pr);ok=g.I();g.getprog(pr,0x8B82,C.byref(ok))
        if not ok.value:
            log=C.create_string_buffer(64000);g.plog(pr,len(log),None,log);raise AssertionError(log.value.decode())
        return pr
    cover=tree.read(SH+'deferred/silCoverF.glsl')+'\nvoid mirrorClip(vec3 p){}\n'
    nb=link([tree.read(SH+'deferred/actorMaskAvatarV.glsl'),tree.read(SH+'avatar/avatarSkinV.glsl')],cover,'#define SIL_COVER_SYSTEM 1\n')
    ne=link(tree.read(SH+'deferred/actorMaskEyesV.glsl'),cover,'#define SIL_COVER_SYSTEM 1\n')
    dll.programs(body,eye,nb,ne,bc,ec,mesh,mc)
    identity=np.eye(4,dtype=np.float32)
    for pr in (body,eye,bc,ec,nb,ne,mesh,mc):
        g.use(pr)
        for name in ('projection_matrix','modelview_matrix','modelview_projection_matrix','texture_matrix0'):
            g.matrix(g.loc(pr,name.encode()),1,0,identity.ctypes.data_as(C.POINTER(g.F)))
        runner.mat3(g.loc(pr,b'normal_matrix'),1,0,np.eye(3,dtype=np.float32).ctypes.data_as(C.POINTER(g.F)))
        if pr in (body,bc,nb):
            palette=np.repeat(identity[:3,None,:],15,axis=1).copy();g.u4fv(g.loc(pr,b'matrixPalette[0]'),45,palette.ctypes.data_as(C.POINTER(g.F)))
        elif pr in (mesh,mc):runner.mat34(g.loc(pr,b'matrixPalette[0]'),1,0,identity[:3].copy().ctypes.data_as(C.POINTER(g.F)))
        g.ui(g.loc(pr,b'diffuseMap'),0);g.uf[2](g.loc(pr,b'sil_cover'),1,0)
    white=runner.texture(np.ones((1,1,4),np.float32),1,1,0x8814)
    fractional=runner.texture(np.array([[[1,1,1,.5]]],np.float32),1,1,0x8814)
    def pixels():
        a=np.zeros((64,64,4),np.float32);g.read(0,0,64,64,0x1908,0x1406,a.ctypes.data);return a[:,:,0]
    results=[]
    for silhouette in (False,True):
        # Silhouette uses a distinct target in production. Execute the native system
        # helper once as that caller before K1, ensuring it neither consumes decisions
        # nor captures/replays styled geometry. The key fixture begins afresh below.
        for part in (0,1,2):
            for label,mode,look,alpha,ready,hidden,selfcase,want,path in [
                ('Replace Dissolve50',1,10,1,1,0,0,'holes',1),('Layer Dissolve50',0,10,.5,1,0,0,'holes',1),
                ('ordinary Layer',0,9,.5,1,0,0,1.,0),('zero',1,9,0,1,0,0,0.,0),('zero no shader',1,9,0,0,0,0,0.,0),
                ('Cover epsilon',1,9,1/255,1,0,0,1/255,1),('Cover half',1,9,.5,1,0,0,.5,1),('Cover full',1,9,1,1,0,0,1.,1),
                ('not ready',1,10,1,0,0,0,1.,0),('self dissolve',1,10,1,1,0,1,'holes',1),('self zero',1,9,0,0,0,1,0.,0),
                ('hidden',1,10,1,1,1,0,0.,0),('derendered',1,10,1,1,2,0,0.,0)]:
                g.bind_fbo(0x8D40,runner.fbo);g.viewport(0,0,64,64);g.fn('glDepthMask',None,g.U)(1);runner.clear(0x1801,0,(g.F*1)(1))
                dll.begin(mode,look,alpha,ready,ready,hidden,selfcase,white)
                dll.beauty(part)
                if silhouette:
                    dll.silhouette(part==1,1);baseline=pixels().copy()
                    dll.silhouette(part==1,0);assert np.array_equal(baseline,pixels()),'I-SILPARITY native system replay'
                assert dll.coverage(part,0,1)==1,(label,part,'replay failure')
                got=pixels();count=dll.count(part,0);contributor=dll.contributor(part,0)
                assert count==(0 if want==0. else 1),(label,part,count)
                assert contributor==path,(label,part,contributor,path)
                if want=='holes':assert np.count_nonzero(got<.001)>20 and np.count_nonzero(got>.1)>20,(label,part,float(got.min()),float(got.max()))
                else:assert np.max(abs(got-want))<1/255+.0001,(label,part,float(got.min()),float(got.max()),want)
                results.append(dict(fixture=label,system=part,silhouette=silhouette,min=float(got.min()),max=float(got.max()),contributors=count,shared=contributor))


    print('PASS system BOM/eyes/transparent real avatar pool -> full K1; I-SILPARITY baseline/current system helper exact; 78 numeric/count fixtures',flush=True)
    dll.legacy_program.argtypes=[C.c_uint,C.c_uint,C.c_int];dll.legacy_beauty.argtypes=[C.c_int,C.c_int]
    ls=runner.program(tree,world,V);lc=runner.program(tree,world+('AL_KEY_COVERAGE_OUT',),V)
    li=runner.program(tree,world+('HAS_SKIN','GHOST_INDEXED_WORLD'),V);lic=runner.program(tree,world+('HAS_SKIN','GHOST_INDEXED_WORLD','AL_KEY_COVERAGE_OUT'),V)
    dll.legacy_program(ls,lc,0);dll.legacy_program(li,lic,1)
    for pr in (ls,lc,li,lic):
        g.use(pr)
        for name in ('projection_matrix','modelview_matrix','modelview_projection_matrix','texture_matrix0'):
            g.matrix(g.loc(pr,name.encode()),1,0,identity.ctypes.data_as(C.POINTER(g.F)))
        runner.mat3(g.loc(pr,b'normal_matrix'),1,0,np.eye(3,dtype=np.float32).ctypes.data_as(C.POINTER(g.F)))
        if pr in (li,lic):runner.mat34(g.loc(pr,b'matrixPalette[0]'),1,0,identity[:3].copy().ctypes.data_as(C.POINTER(g.F)))
    for silhouette in (False,True):
        for variant in (0,1,2):
            for label,mode,look,alpha,ready,hidden,selfcase,want,path in [
                ('Replace Dissolve50',1,10,1,1,0,0,'holes',1),('Layer Dissolve50',0,10,.5,1,0,0,'holes',1),
                ('ordinary Layer',0,9,.5,1,0,0,1.,0),('zero',1,9,0,1,0,0,0.,0),('zero no shader',1,9,0,0,0,0,0.,0),
                ('Cover epsilon',1,9,1/255,1,0,0,1/255,1),('Cover half',1,9,.5,1,0,0,.5,1),('Cover full',1,9,1,1,0,0,1.,1),
                ('not ready',1,10,1,0,0,0,1.,0),('hidden',1,10,1,1,1,0,0.,0),('derendered',1,10,1,1,2,0,0.,0)]:
                g.bind_fbo(0x8D40,runner.fbo);g.viewport(0,0,64,64);g.fn('glDepthMask',None,g.U)(1);runner.clear(0x1801,0,(g.F*1)(1))
                dll.begin(mode,look,alpha,ready,ready,hidden,selfcase,white)
                assert dll.legacy_beauty(variant,0)==1,(label,variant,'legacy draw failed')
                assert dll.coverage(0,1,1)==1
                got=pixels();count=dll.count(0,1);contributor=dll.contributor(0,1)
                assert count==(0 if want==0 else 1),(label,variant,count)
                assert contributor==path,(label,variant,contributor,path)
                if want=='holes':assert np.count_nonzero(got<.001)>20 and np.count_nonzero(got>.1)>20,(label,variant,float(got.min()),float(got.max()))
                else:assert np.max(abs(got-want))<1/255+.0001,(label,variant,float(got.min()),float(got.max()),want)
                results.append(dict(fixture='legacy '+label,variant=variant,silhouette=silhouette,min=float(got.min()),max=float(got.max()),contributors=count,shared=contributor))
    print('PASS world legacy full drawGeometryGhost static/rigged/indexed -> actual capture hooks -> real K1; Dissolve Replace/Layer holes, ordinary Layer, zero/epsilon/half/full Cover, not-ready, hidden/derendered; counts and R8 readback',flush=True)

    # Body/eyes/transparent system part plus rigged attachment on the SAME avatar,
    # captured in one beauty invocation and resolved by the real K1 union.
    for part in (0,1,2):
        for mode,look,alpha,ready,hidden,selfcase,want in [
            (1,10,1,1,0,0,'holes'),(0,10,.5,1,0,0,'holes'),(0,9,.5,1,0,0,1.),
            (1,9,0,1,0,0,0.),(1,9,0,0,0,0,0.),(1,9,1/255,1,0,0,1-(1-1/255)**2),
            (1,9,.5,1,0,0,.75),(1,9,1,1,0,0,1.),(1,10,1,0,0,0,1.),
            (1,10,1,1,0,1,'holes'),(1,9,0,0,0,1,0.),(1,10,1,1,1,0,0.),(1,10,1,1,2,0,0.)]:
            for silhouette in (False,True):
                g.bind_fbo(0x8D40,runner.fbo);g.fn('glDepthMask',None,g.U)(1);runner.clear(0x1801,0,(g.F*1)(1))
                dll.begin(mode,look,alpha,ready,ready,hidden,selfcase,white);dll.beauty(part);assert dll.legacy_beauty(1,0)==1
                if silhouette:dll.silhouette(part==1,0)
                assert dll.coverage(part,3,1)==1
                count=0 if want==0 else 1
                assert dll.count(part,0)==count and dll.count(0,1)==count,('BOM+attachment',part,mode,look,alpha,dll.count(part,0),dll.count(0,1))
                got=pixels()
                if want=='holes':assert np.count_nonzero(got<.001)>20 and np.count_nonzero(got>.1)>20
                else:assert np.max(abs(got-want))<1/255+.0001,('BOM+attachment coverage',part,alpha,float(got.min()),float(got.max()),want)
                results.append(dict(fixture='same avatar BOM+attachment',part=part,mode=mode,look=look,alpha=alpha,ready=ready,hidden=hidden,self=selfcase,silhouette=silhouette,coverage_min=float(got.min()),coverage_max=float(got.max()),system_count=dll.count(part,0),attachment_count=dll.count(0,1)))
    # Distortion retains authored transparent holes on system hair and rigged mesh.
    texels=np.ones((64,64,4),np.float32);texels[20:44,20:44,3]=0
    cutout=runner.texture(texels,64,64,0x8814)
    for silhouette in (False,True):
        dll.begin(1,9,1,1,1,0,0,cutout);dll.distortion();dll.beauty(1);assert dll.legacy_beauty(1,1)==1
        if silhouette:dll.silhouette(1,0)
        assert dll.coverage(1,3,1)==1
        assert dll.count(1,0)==1 and dll.count(0,1)==1
        assert np.count_nonzero(pixels()<.001)>20 and np.count_nonzero(pixels()>.1)>20
    print('PASS D55 same-avatar BOM body/eyes/transparent plus rigged attachment, silhouette OFF/ON; continuous Cover and authored Distort holes; counts independently per typed component',flush=True)
    # Full shared-PBR shader family, including actual vertex and material modules.
    from env_only_gl_check import parity_class, COMMON
    namespace=dict(G=G,gl=g,C=C,np=np,Path=Path,OUT=OUT,
        LEVEL1={'deferred/globalF.glsl','deferred/deferredUtil.glsl','deferred/gbufferUtil.glsl','deferred/shadowUtil.glsl'})
    Base=parity_class(namespace);prunner=Base('WORKTREE')
    def pbr_program(variant,coverage):
        defs='#define REFMAP_LEVEL 1\n#define HAS_ACTOR_FX 1\n#define SHARED_ACTOR_FX_REPLAY 1\n#define SHARED_ACTOR_FX_ALPHA_MODE 2\n'
        if variant:defs+='#define HAS_SKIN 1\n'
        if variant==2:defs+='#define SHARED_ACTOR_FX_SLOT_FILTER 1\n#define GLTF_INDEXED_CHANNELS 2\n'
        if coverage and os.environ.get('ACTOR_KEY_WP1B_MUTATION')!='pbr-colour':defs+='#define AL_KEY_COVERAGE_OUT 1\n'
        pr=g.program()
        for module in COMMON+['deferred/reflectionProbeF.glsl','deferred/screenSpaceReflUtil.glsl','alchemy/avatarLightResponseF.glsl','alchemy/actorFxDissolveF.glsl','alchemy/actorFxF.glsl','class2/deferred/sharedActorFxPbrF.glsl']:
            g.attach(pr,prunner.compile(prunner.source(module),defs+'#define FRAGMENT_SHADER 1\n',module))
        for module in ['class1/deferred/sharedActorFxPbrV.glsl','deferred/textureUtilV.glsl']+(['avatar/objectSkinV.glsl'] if variant else []):
            g.attach(pr,prunner.compile(prunner.source(module),defs+'#define VERTEX_SHADER 1\n',module,0x8B31))
        g.link(pr);ok=g.I();g.getprog(pr,0x8B82,C.byref(ok))
        if not ok.value:
            log=C.create_string_buffer(64000);g.plog(pr,len(log),None,log);raise AssertionError(log.value.decode())
        return pr
    dll.pbr_program.argtypes=[C.c_uint,C.c_uint,C.c_int]
    for variant in (0,1,2):
        pb=pbr_program(variant,False);pc=pbr_program(variant,True);dll.pbr_program(pb,pc,variant)
        for pr in (pb,pc):
            g.use(pr)
            for name in ('projection_matrix','modelview_matrix','modelview_projection_matrix','texture_matrix0'):
                g.matrix(g.loc(pr,name.encode()),1,0,identity.ctypes.data_as(C.POINTER(g.F)))
            runner.mat3(g.loc(pr,b'normal_matrix'),1,0,np.eye(3,dtype=np.float32).ctypes.data_as(C.POINTER(g.F)))
            if variant:runner.mat34(g.loc(pr,b'matrixPalette[0]'),1,0,identity[:3].copy().ctypes.data_as(C.POINTER(g.F)))
            for name in ('texture_base_color_transform','texture_normal_transform','texture_metallic_roughness_transform','texture_emissive_transform'):
                g.u4fv(g.loc(pr,(name+'[0]').encode()),2,np.array([0,0,1,0,0,0,1,0],np.float32).ctypes.data_as(C.POINTER(g.F)))
            for name in ('sun_dir','moon_dir'):g.uf[3](g.loc(pr,name.encode()),0,0,1)
            g.ui(g.loc(pr,b'sun_up_factor'),1)
        for look in (9,2):
            for label,mode,alpha,ready,pbrReady,hidden,selfcase,want,path in [
                ('ordinary Layer alpha.5',0,.4,1,1,0,0,.5,1),('PBR not ready',0,.4,1,0,0,0,.5,0),
                ('mixed owner PBR',0,.4,1,1,0,0,.5,1),('Replace PBR',1,1,1,1,0,0,.5,1),
                ('Cover half PBR',1,.5,1,1,0,0,.25,1),('Cover epsilon PBR',1,1/255,1,1,0,0,.5/255,1),
                ('zero PBR',1,0,1,1,0,0,0.,0),('zero PBR not ready',1,0,0,0,0,0,0.,0),
                ('hidden PBR',1,1,1,1,1,0,0.,0),('derendered PBR',1,1,1,1,2,0,0.,0),
                ('self Layer PBR',0,.4,1,1,0,1,.5,1)]:
                g.bind_fbo(0x8D40,runner.fbo);g.viewport(0,0,64,64);g.fn('glDepthMask',None,g.U)(1);runner.clear(0x1801,0,(g.F*1)(1))
                dll.begin(mode,look,alpha,ready,pbrReady,hidden,selfcase,fractional)
                dll.mesh_beauty(variant+1)
                assert dll.coverage(0,1,1)==1,(label,variant,'replay failure')
                got=pixels();count=dll.count(0,1);contributor=dll.contributor(0,1)
                assert count==(0 if want==0 else 1),(label,variant,count)
                assert contributor==path,(label,variant,contributor,path)
                assert np.max(abs(got-want))<1/255+.0001,(label,variant,float(got.min()),float(got.max()),want)
                results.append(dict(fixture=label,variant=variant,look=look,min=float(got.min()),max=float(got.max()),contributors=count,shared=contributor))
        # Ordinary-Layer legacy attachment on that SAME owner remains native.
        dll.begin(0,9,.4,1,1,0,0,fractional);dll.mesh_beauty(0);assert dll.coverage(0,1,1)==1
        assert dll.count(0,1)==1 and dll.contributor(0,1)==0 and np.max(abs(pixels()-.5))<1/255

        for label,mode,alpha,ready,pbrReady,hidden,want,pbrShared,legacyShared,count in [
            ('mixed ordinary Layer',0,.4,1,1,0,.75,1,0,1),
            ('mixed PBR not ready',0,.4,1,0,0,.75,0,0,1),
            ('mixed legacy not ready',1,1,0,1,0,.75,1,0,1),
            ('mixed PBR not ready Replace',1,1,1,0,0,.75,0,1,1),
            ('mixed zero no shaders',1,0,0,0,0,0.,0,0,0),
            ('mixed hidden',1,1,1,1,1,0.,0,0,0)]:
            dll.begin(mode,9,alpha,ready,pbrReady,hidden,0,fractional);dll.mixed_beauty(variant);assert dll.coverage(0,2,1)==1
            assert dll.count(0,2)==count and dll.count(1,2)==count,(label,variant,dll.count(0,2),dll.count(1,2))
            assert dll.contributor(0,2)==pbrShared and dll.contributor(1,2)==legacyShared
            assert np.max(abs(pixels()-want))<1/255+.0001,(label,variant,float(pixels().min()),float(pixels().max()))
            results.append(dict(fixture=label,variant=variant,coverage=float(pixels()[32,32]),pbr_contributors=dll.count(0,2),legacy_contributors=dll.count(1,2)))
    dll.reverse_winding(1);dll.begin(0,9,.4,1,1,0,0,fractional);dll.mesh_beauty(3);assert dll.coverage(0,1,1)==1
    assert dll.count(0,1)==1 and np.max(abs(pixels()-.5))<1/255+.0001,'indexed double-sided backface capture'
    dll.reverse_winding(0)
    print('PASS R42 full production PBR command helper/scalar static/rigged/indexed binder -> capture -> K1 replay: ordinary Layer alpha .5 Chrome/Hologram, PBR not-ready, mixed legacy, Replace, zero, continuous Cover, hidden/derendered, self; numeric coverage AND counts',flush=True)

    # A9 retains exact inputs even for ordinary-Layer draws that never contribute.
    dll.begin(0,9,.5,1,1,0,0,white);dll.beauty(0)
    assert dll.captured_nonbearing()==1,'ordinary Layer system exact input record'
    dll.begin(0,9,.5,1,1,0,0,white);dll.legacy_beauty(1,1)
    assert dll.captured_nonbearing()==1,'ordinary Layer legacy exact input record'

    # Alpha toggle and refusal/recovery execute the complete K1 method, not a replica.
    dll.begin(1,9,.5,1,1,0,0,white);dll.beauty(1);assert dll.coverage(1,0,0)==1
    assert dll.count(1,0)==0 and np.max(pixels())==0,'alpha OFF system capture'
    dll.begin(1,9,.5,1,1,0,0,white);dll.legacy_beauty(1,1);assert dll.coverage(0,1,0)==1
    assert dll.count(0,1)==0 and np.max(pixels())==0,'alpha OFF legacy capture'
    dll.begin(0,9,.5,1,1,0,0,fractional);dll.mesh_beauty(3);assert dll.coverage(0,1,0)==1
    assert dll.count(0,1)==0 and np.max(pixels())==0,'alpha OFF PBR capture'
    dll.begin(1,9,.5,1,1,0,0,white);dll.beauty(0);assert dll.failed_k1()==1
    dll.blocked(1);dll.beauty(0);dll.blocked(0)
    dll.begin(1,9,.5,1,1,0,0,white);assert dll.absent_capture()==1;dll.beauty(0);assert dll.coverage(0,0,1)==1
    assert dll.count(0,0)==1 and np.max(abs(pixels()-.5))<1/255+.0001
    dll.poison_inputs();assert dll.coverage(0,0,1)==1
    assert dll.count(0,0)==1 and np.max(abs(pixels()-.5))<1/255+.0001,'captured palette/material/time/alpha must survive changed beauty program inputs'
    assert dll.off_baseline()==1,'OFF recorder allocation baseline' 
    print('PASS same-owner mixed PBR/legacy readiness in both directions; alpha OFF system/legacy/PBR; failed K1 disarm, suppressed frame and fresh recovery; OFF allocation baseline',flush=True)
    (OUT/'results.json').write_text(json.dumps(results,indent=2))
    dll.cleanup()
    print('PASS W-K8 production avatar pool -> typed beauty decisions -> real system beauty capture -> native/styled coverage; BOM body/transparent/rigid eyes, Replace/Layer Dissolve holes, ordinary Layer, zero with/without shaders, continuous Cover, not-ready, canonical self, hidden/derendered; numeric coverage AND contributor counts',flush=True)

    if not os.environ.get('ACTOR_KEY_WP1B_MUTATION'):
        import sys
        for mutation in ('native-leak','system-leak','pbr-colour'):
            log=OUT/('negative_'+mutation+'.log')
            with log.open('w',encoding='utf-8') as stream:
                run=subprocess.run([sys.executable,str(Path(__file__).resolve())],env={**os.environ,'ACTOR_KEY_WP1B_MUTATION':mutation},stdout=stream,stderr=subprocess.STDOUT,timeout=180)
            assert run.returncode!=0,'negative control survived: '+mutation
            diagnostic=log.read_text(encoding='utf-8')
            assert 'AssertionError:' in diagnostic and 'RuntimeError:' not in diagnostic and 'LINK :' not in diagnostic,diagnostic
            print('PASS negative control rejected:',mutation,flush=True)

if __name__=='__main__':
    import sys,traceback
    try:main()
    except BaseException:
        traceback.print_exc();sys.stdout.flush();sys.stderr.flush();C.windll.kernel32.TerminateProcess(C.c_void_p(-1),1)
    # Core DLL globals expect the full viewer shutdown services. The fixture releases
    # its resources explicitly above; terminate only this isolated test process.
    sys.stdout.flush();sys.stderr.flush();C.windll.kernel32.TerminateProcess(C.c_void_p(-1),0)

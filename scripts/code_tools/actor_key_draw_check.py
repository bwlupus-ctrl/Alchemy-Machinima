"""Complete real drawGeometryGhost execution against recorded external GL seams.

Production sweeps, blend/depth/colour state and uniforms execute unchanged.
Synthetic descriptors supply external scene/mesh/material lookups. The driver
fixture consumes the recorded commands with the full production shaders.
"""
import json
import re
import subprocess
from pathlib import Path
from native_check_tools import compile_run
from relight_area_policy_check import method

ROOT=Path(__file__).resolve().parents[2]
NV=ROOT/'indra/newview'
OUT=ROOT/'.tmp/ckey_20261102/wp2/draw'


def native():
    OUT.mkdir(parents=True,exist_ok=True)
    source=(NV/'llactormover.cpp').read_text(encoding='utf-8')
    baseline=OUT/'baseline.cpp'
    baseline.write_bytes(subprocess.check_output(['git','show','ab2329ba39f:indra/newview/llactormover.cpp']))
    prefix=Path(__file__).with_name('actor_key_draw_fixture.h').read_text(encoding='utf-8')
    prefix=prefix.replace('// GP_INSERT',method(NV/'llactormover.h','struct GhostDrawParams')+';')
    prefix+=method(NV/'llactormover.cpp','enum EGhostStyle')+';\n'
    prefix+='struct ALGhostStudio {\n'+ '\n'.join(method(NV/'alghoststudio.h', sig) for sig in (
        '    static S32 remapRetiredOverlayStyle(', '    static bool isOverlayStyle(',
        '    static S32 resolveOverlayStyle('))+'\n};\n'
    production=method(NV/'llactormover.cpp','S32 drawGeometryGhost(')
    for name in sorted(set(re.findall(r'\bsGhost\w+',production))):
        match=re.search(r'\b'+name+r'\s*\(\s*"([^"]+)"\s*\)',source)
        assert match,name
        prefix+='LLStaticHashedString '+name+'("'+match[1]+'");\n'
    old=method(baseline,'S32 drawGeometryGhost(').replace('S32 drawGeometryGhost(','S32 drawGeometryGhostBaseline(',1)
    fixture=prefix+old+'\n'+production+r'''
bool equal(const std::vector<Trace>& a,const std::vector<Trace>& b){if(a.size()!=b.size())return false;
 for(size_t i=0;i<a.size();++i){auto& x=a[i];auto& y=b[i];
 if(x.program!=y.program||x.vb!=y.vb||x.blend!=y.blend||x.uniforms!=y.uniforms||x.state.depth!=y.state.depth||x.state.write!=y.state.write||x.state.colour!=y.state.colour||x.state.func!=y.state.func||x.state.cs!=y.state.cs||x.state.cd!=y.state.cd||x.state.as!=y.state.as||x.state.ad!=y.state.ad)return false;}return true;}
void reset(){trace.clear();state={};enabled.clear();LLGLSLShader::sCurBoundShaderPtr=nullptr;
 for(auto* sh:{&gActorGhostProgram,&gSkinnedActorGhostProgram,&gHighlightProgram,&gHighlightSkin,&sStudioKeyPrograms->gActorGhostKeyFillProgram,&sStudioKeyPrograms->gSkinnedActorGhostKeyFillProgram})sh->u.clear();}
void output(int look,float alpha,bool fill,bool stat){std::cout<<"{\"look\":"<<look<<",\"alpha\":"<<alpha<<",\"fill\":"<<fill<<",\"static\":"<<stat<<",\"draws\":[";bool first=true;
 for(auto& t:trace){if(!first)std::cout<<',';first=false;std::cout<<"{\"program\":"<<t.program<<",\"vb\":"<<t.vb<<",\"blend\":"<<t.blend<<",\"depth\":"<<t.state.depth<<",\"write\":"<<t.state.write<<",\"colour\":"<<t.state.colour<<",\"func\":"<<t.state.func<<",\"factors\":["<<t.state.cs<<','<<t.state.cd<<','<<t.state.as<<','<<t.state.ad<<"],\"uniforms\":{";
 bool uf=true;for(auto& u:t.uniforms){if(!uf)std::cout<<',';uf=false;std::cout<<'"'<<u.first<<"\":[";for(size_t i=0;i<u.second.size();++i){if(i)std::cout<<',';std::cout<<u.second[i];}std::cout<<']';}std::cout<<"}}";}std::cout<<"]}\n";}
int main(){gActorGhostProgram.mRiggedVariant=&gSkinnedActorGhostProgram;gHighlightProgram.mRiggedVariant=&gHighlightSkin;sStudioKeyPrograms->gActorGhostKeyFillProgram.mRiggedVariant=&sStudioKeyPrograms->gSkinnedActorGhostKeyFillProgram;
 LLVOAvatar avatar;int skin=1;LLVertexBuffer solid,blend,glow;blend.id=2;glow.id=3;
 LLFetchedGLTFMaterial material;LLViewerTexture texture;material.mEmissiveTexture=&texture;
 LLDrawInfo di,bi,gi;di.mAvatar=&avatar;di.mSkinInfo=&skin;di.mVertexBuffer=&solid;bi=gi=di;bi.mVertexBuffer=&blend;gi.mVertexBuffer=&glow;gi.mGLTFMaterial=&material;
 LLFace face;face.vb=&solid;face.mDrawInfo=&di;std::vector<LLActorMover::GhostStaticFace> faces={{&face,0,.5f,false,{1}}};
 for(bool stat:{false,true})for(int look:{1,10,64,68,69,72,75,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100})for(float alpha:{.4f,1.f}){
 std::vector<LLActorMover::GhostBatch>batches;if(!stat)batches={{&di,0},{&bi,1},{&gi,2}};
 LLActorMover::GhostDrawParams gp;gp.mIsClone=look==1;gp.mDissolveProgress=.5f;gp.mDistort=3;gp.mDistortAmount=.5f;gp.mPhase=.73f;gp.mKeyFillColour={0.f,177.f/255.f,64.f/255.f};
 StudioLookFrame frame;frame.scene=look==64;bool complete=false;
 reset();if(look<85){drawGeometryGhostBaseline(&avatar,batches,{}, {1,1,1,1},alpha,look,gp,stat?&faces:nullptr,&complete,&frame);assert(complete);}auto reference=trace;
 reset();drawGeometryGhost(&avatar,batches,{}, {1,1,1,1},alpha,look,gp,stat?&faces:nullptr,&complete,&frame);assert(complete&&!trace.empty());
 if(look<85)assert(equal(reference,trace));
 else {
  assert(gp.mEffectFps==0.f && gp.mDissolveProgress==.5f);
  int beauty=0;
  for(auto& d:trace){assert(d.vb==1);if(!d.state.colour)continue;++beauty;
   assert(d.program==(stat?1:2));assert(d.uniforms.at("ghostLook")[0]==look);
   assert(d.uniforms.at("ghostParams")[0]==0 && d.uniforms.at("ghostParams")[1]==1);
   assert(d.uniforms.at("ghostAux")[1]==1); // real diffuse RGB, not the white fallback
   assert(d.uniforms.at("ghostDissolveProgress")[0]==.5f);
   if(look==90||look==91||look==99)assert(d.uniforms.at("ghostScreenSize")[0]==256);
  }
  assert(beauty==1);reference=trace;
 }
 output(look,alpha,false,stat);
 gp.mKeyFill=true;reset();drawGeometryGhost(&avatar,batches,{}, {1,1,1,1},alpha,look,gp,stat?&faces:nullptr,&complete,&frame);assert(complete&&!trace.empty());
 for(auto& t:trace){assert(t.program==3||t.program==4);if(t.state.colour)assert(!(t.blend&&t.state.cd==LLRender::BF_ONE));assert(t.vb!=3);assert(t.uniforms.at("ghostSceneValid")[0]==1);}
 output(look,alpha,true,stat);
 gp.mKeyFill=false;trace.clear();drawGeometryGhost(&avatar,batches,{}, {1,1,1,1},alpha,look,gp,stat?&faces:nullptr,&complete,&frame);
 assert(complete&&equal(reference,trace)); // ON -> OFF, with no program/state reset

 }
}
'''
    result=compile_run(OUT,fixture,'draw_real')
    result.check_returncode()
    records=[json.loads(line) for line in result.stdout.splitlines()]
    (OUT/'commands.json').write_text(json.dumps(records,indent=2))
    assert 'studio_draw_displaced' not in production and 'ghostDisplace' not in production
    print('PASS I-CLONEPARITY C++: incoming-main exact legacy traces; new 85-100 real textured solid sweep, actual defaults, one beauty, no blend/glow; rigged/static .4/1; fill variants; no displaced sweep',flush=True)
    for name,old,new in (
        ('fill_emission', 'if (!gp.mKeyFill && (overlay_mask & GHOST_COVERAGE_RIGGED_GLOW))', 'if (overlay_mask & GHOST_COVERAGE_RIGGED_GLOW)'),
        ('fill_program', 'if (gp.mKeyFill) fx_base =', 'if (false) fx_base ='),
    ):
        assert fixture.count(old)==1,name
        mutant=compile_run(OUT,fixture.replace(old,new),name)
        assert mutant.returncode,name
        print('PASS negative control:',name,flush=True)
    return records


def driver(records):
    import ctypes as C
    import numpy as np
    import relight_3d_gl as G
    from clone_looks_gl_check import Runner
    runner=Runner(256,256)
    g=runner.gl
    cache={}
    real_draw=g.draw
    integer_names={'ghostLook','ghostIsClone','ghostSlot','ghostDistort','ghostUseVertexAlpha',
                   'ghostAlphaCutoffMode','ghostWorldLinear','ghostSceneValid','alrEnabled','ghostGlowOnly','ghostDecodeScrub'}
    def render(record,colour=(0.,177/255,64/255),reference=False,**kwargs):
        defs=(() if record['static'] else ('HAS_SKIN',))+ (('AL_KEY_FILL',) if record['fill'] else ())
        key=(defs,reference)
        if key not in cache:
            cache[key]=runner.program(G.SourceTree('ab2329ba39f' if reference else 'WORKTREE'),defs)
        program=cache[key]
        g.fn('glColorMask',None,g.U,g.U,g.U,g.U)(1,1,1,1)
        g.fn('glDepthMask',None,g.U)(1)
        g.disable(0x0BE2)
        g.bind_fbo(0x8D40,runner.fbo)
        runner.clear(0x1801,0,(g.F*1)(1.))
        def submit(mode,first,count):
            for command in record['draws']:
                assert command['program']==(3 if record['static'] else 4) if record['fill'] else command['program']==(1 if record['static'] else 2)
                g.use(program)
                for name,values in command['uniforms'].items():
                    loc=g.loc(program,name.encode())
                    if name in integer_names:
                        g.ui(loc,int(values[0]))
                    elif len(values)<=4:
                        g.uf[len(values)](loc,*values)
                if record['fill']:
                    g.uf[3](g.loc(program,b'ghostKeyFill'),*colour)
                (g.fn('glEnable',None,g.U) if command['blend'] else g.disable)(0x0BE2)
                (g.fn('glEnable',None,g.U) if command['depth'] else g.disable)(0x0B71)
                g.fn('glDepthMask',None,g.U)(command['write'])
                g.fn('glDepthFunc',None,g.U)(0x0201 if command['func']==3 else 0x0203)
                value=command['colour']
                g.fn('glColorMask',None,g.U,g.U,g.U,g.U)(value,value,value,value)
                factors=[(0,1,0x0302,0x0303)[v] for v in command['factors']]
                g.fn('glBlendFuncSeparate',None,g.U,g.U,g.U,g.U)(*factors)
                real_draw(mode,first,count)
            g.fn('glColorMask',None,g.U,g.U,g.U,g.U)(1,1,1,1)
        g.draw=submit
        try:
            return runner.render(program,record['look'],opacity=record['alpha'],**kwargs)
        finally:
            g.draw=real_draw
    for stat in (False,True):
        selected=[r for r in records if bool(r['static'])==stat]
        for normal,fill in zip(selected[::2],selected[1::2]):
            a=render(normal)
            if normal['look'] < 85:
                b=render(normal,reference=True)
                assert np.array_equal(a,b),(stat,normal['look'],'baseline/current pixels')
            white=render(fill,(1,1,1))
            key=render(fill)
            assert np.any(white[...,0]>0),(stat,normal['look'],'empty footprint')
            assert np.max(np.abs(key[...,:3]-white[...,:3]*np.array([0,177/255,64/255])))<2e-6
            assert np.array_equal(white[...,3],key[...,3]),'colour changed footprint alpha'
            black=render(fill,(0,0,0))
            assert np.all(black[...,:3]==0),'inverted fill emitted light'
            # Legacy IDs 68/72 resolve to Clone, whose emission pass is omitted
            # by fill just like explicit Clone. The C++ fixture checks parity.
            if normal['look'] not in (1,68,72):
                assert np.array_equal(a[...,3],key[...,3]),(stat,normal['look'],'normal/fill footprint')
        print('PASS I-CLONEPARITY/fill WGL replay of real drawGeometryGhost commands:',
              'static' if stat else 'rigged', 'baseline/current exact; key/white/invert fills; Dissolve/Distort footprint; emission adds nothing',flush=True)
    # The production command source includes the real Glass sweep. Retain the
    # already composited target for the near Glass, just as Studio does.
    glass=next(r for r in records if not r['static'] and not r['fill'] and r['look']==64 and r['alpha']==1)
    clone=next(r for r in records if not r['static'] and not r['fill'] and r['look']==1 and r['alpha']==1)
    for rear in (clone,glass):
        render(rear,scene=rear['look']==64,composite=True)
        current=render(glass,scene=True,keep=True,composite=True,shift=.15)
        render(rear,scene=rear['look']==64,composite=True,reference=True)
        old=render(glass,scene=True,keep=True,composite=True,shift=.15,reference=True)
        assert np.array_equal(current,old),'front Glass over rear Clone/Glass parity'
    assert g.fn('glGetError',g.U)()==0
    print('PASS I-CLONEPARITY: real sweep commands, front Glass over rear Clone/Glass with fresh Key-colour source',flush=True)


if __name__=='__main__':
    driver(native())

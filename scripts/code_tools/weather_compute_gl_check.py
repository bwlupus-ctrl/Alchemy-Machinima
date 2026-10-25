"""WP3 Stage A: real GL production-loader compile/link/dispatch/readback and identity.

Run this before particle work. --source-ref 28a5d96de19 rejects the pre-WP3 loader.
"""
import os
os.environ['PYTHONUTF8']='1'
import argparse
import ctypes as C
import subprocess
import io
import tarfile
from pathlib import Path
from relight_3d_gl import GL, SourceTree, SHADERS
from weather_cpu_check import ROOT
from weather_compute_fixture import build, function


def run(ref='WORKTREE'):
    gl=GL();print('WGL',gl.renderer,flush=True)
    out=ROOT/'.tmp/weather_port/compute'/str(os.getpid())
    tree=SourceTree(ref);native=build(tree,out,gl)
    cls=out/'class1';cls.mkdir(exist_ok=True)
    source='''#if !defined(COMPUTE_SHADER) || defined(VERTEX_SHADER)
#error wrong compute classification
#endif
layout(local_size_x=128) in;
layout(std430,binding=0) buffer Values { uint values[]; };
void main(){uint i=gl_GlobalInvocationID.x;values[i]=i*7u+11u;}
'''
    (cls/'compute.glsl').write_text(source)
    p=native.program(b'compute.glsl',0x91B9,0);assert p,'production compute link failed'
    assert native.bind_attributes(p,1)==0,'compute bind touches graphics attributes'
    assert native.bind_attributes(p,0)>0,'attribute sentinel is disconnected'
    discard=native.program(b'compute.glsl',0x91B9,0)
    assert discard and native.unload_attributes(discard)==0,'compute unload touches graphics attributes'
    assert native.feature_count()==0,'graphics features attached to compute'
    assert native.program(b'compute.glsl',0x91B9,1)==0,'mixed graphics/compute accepted'
    buf=C.c_uint();gl.gen_buf(1,C.byref(buf));gl.bind_buf(0x90D2,buf)
    gl.buffer_data(0x90D2,128*4,None,0x88E8)
    gl.fn('glBindBufferBase',None,C.c_uint,C.c_uint,C.c_uint)(0x90D2,0,buf)
    gl.use(p);gl.fn('glDispatchCompute',None,C.c_uint,C.c_uint,C.c_uint)(1,1,1)
    gl.fn('glMemoryBarrier',None,C.c_uint)(0x2000|0x200)
    values=(C.c_uint*128)()
    gl.fn('glGetBufferSubData',None,C.c_uint,C.c_ssize_t,C.c_ssize_t,C.c_void_p)(0x90D2,0,C.sizeof(values),values)
    assert list(values)==[i*7+11 for i in range(128)],'dispatch readback'
    a=native.load(b'compute.glsl',0x91B9,0)
    assert a==native.load(b'compute.glsl',0x91B9,0),'identical permutation not cached'
    assert a!=native.load(b'compute.glsl',0x91B9,1),'permutation cache collision'
    (cls/'stage.glsl').write_text('void main(){}\n')
    v=native.load(b'stage.glsl',0x8B31,0);f=native.load(b'stage.glsl',0x8B30,0)
    assert v and f and v!=f,'stage cache collision'
    native.language(4,10);v400=native.load(b'stage.glsl',0x8B31,0)
    native.language(4,60)
    assert v400 and v400!=v,'version cache collision'
    (cls/'bad.glsl').write_text('this deliberately fails compilation\n')
    assert native.program(b'bad.glsl',0x91B9,0)==0 and native.instance_count()==0,'failed program retained live state'
    native.capability(0,1)
    assert native.load(b'compute.glsl',0x91B9,0)==0,'unsupported GL loaded cached compute'
    assert native.program(b'compute.glsl',0x91B9,0)==0,'unsupported GL linked compute'
    native.capability(1,0)
    assert native.program(b'compute.glsl',0x91B9,0)==0,'missing entry point accepted'
    # Reinitialize entry points using a fresh fixture, also exercising reload cleanup.
    gl.use(0);gl.fn('glDeleteProgram',None,C.c_uint)(p)
    native.clear();assert not gl.fn('glIsShader',C.c_ubyte,C.c_uint)(a),'cache clear leaked unattached shader'
    native=build(tree,out/'identity',gl);cls=out/'identity/class1';cls.mkdir()
    # The old production loader assembles every existing shader with identical inputs.
    # Source is compared via glGetShaderSource, including failed standalone utility
    # modules (which legitimately lack a main).
    oldtree=SourceTree('330d012e161')
    class OldTree:
        def read(self,path):
            if path.endswith('llshadermgr.cpp'):
                old=oldtree.read(path)
                # Retain the baseline assembly verbatim inside the fixture API.
                current=tree.read(path)
                current=current.replace(function(current,'GLuint LLShaderMgr::loadShaderFile('),
                    function(old,'GLuint LLShaderMgr::loadShaderFile('))
                current=current.replace(function(current,'void LLShaderMgr::initAttribsAndUniforms('),
                    function(old,'void LLShaderMgr::initAttribsAndUniforms('))
                return current
            return tree.read(path)
    old=build(OldTree(),out/'old',gl);oldcls=out/'old/class1';oldcls.mkdir()
    getsource=gl.fn('glGetShaderSource',None,C.c_uint,C.c_int,C.c_void_p,C.c_char_p)
    archive=subprocess.check_output(['git','archive','28a5d96de19',SHADERS],cwd=ROOT,timeout=30)
    files=tarfile.open(fileobj=io.BytesIO(archive))
    count=0
    for member in files:
        path=member.name
        if not path.endswith('.glsl'):continue
        # Use a valid wrapper carrying the complete original bytes as comments;
        # preserve the EXTRA_CODE marker position and exact preamble assembly.
        stage=0x8B31 if path.endswith('V.glsl') else 0x8B30
        content=files.extractfile(member).read().decode('utf-8')
        # Existing sources themselves are checked unchanged; preamble emitted
        # before/at their marker is measured with a small valid graphics shader.
        assert tree.read(path)==content,'existing shader modified: '+path
        marker='/*[EXTRA_CODE_HERE]*/\n' if '[EXTRA_CODE_HERE]' in content else ''
        fixture=marker+('void main(){gl_Position=vec4(0);}\n' if stage==0x8B31 else 'out vec4 frag_color;void main(){frag_color=vec4(1);}\n')
        (cls/'identity.glsl').write_text(fixture);(oldcls/'identity.glsl').write_text(fixture)
        for perm in (0,1):
            x=native.load(b'identity.glsl',stage,perm);y=old.load(b'identity.glsl',stage,perm)
            assert x and y
            bx=C.create_string_buffer(65536);by=C.create_string_buffer(65536)
            getsource(x,len(bx),None,bx);getsource(y,len(by),None,by)
            assert bx.value==by.value,'graphics preamble changed '+path
        count+=1
    # Actual existing copy program, unchanged source, native attribute binding
    # and native sampler mapping; compare to the immutable identity baseline.
    programs=[]
    for lib,root,src in ((native,cls,tree),(old,oldcls,oldtree)):
        (root/'interface').mkdir(exist_ok=True)
        for stage in ('V','F'):
            name='interface/copy'+stage+'.glsl'
            (root/name).write_text(src.read(SHADERS+'/class1/'+name),encoding='utf-8')
        lib.graphics.argtypes=[C.c_char_p,C.c_char_p];lib.graphics.restype=C.c_uint
        program=lib.graphics(b'interface/copyV.glsl',b'interface/copyF.glsl');assert program,'existing graphics program fails native link'
        assert gl.fn('glGetAttribLocation',C.c_int,C.c_uint,C.c_char_p)(program,b'position')==0,'existing position attribute moved'
        samplers=[]
        for name in (b'diffuseMap',b'depthMap'):
            value=C.c_int(-1);location=gl.loc(program,name);assert location>=0,'existing sampler missing'
            gl.fn('glGetUniformiv',None,C.c_uint,C.c_int,C.c_void_p)(program,location,C.byref(value));samplers.append(value.value)
        programs.append(samplers)
    assert programs==[[0,1],[0,1]],'existing sampler assignments changed '+str(programs)
    print('PASS actual existing graphics copy program / position attribute / diffuse+depth sampler mappings',flush=True)
    print(f'PASS production compute dispatch/readback, capability negatives, cache/reload; {count} existing sources/preambles identical',flush=True)
    assert gl.fn('glGetError',C.c_uint)()==0,'GL error'


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-ref',default='WORKTREE')
    run(p.parse_args().source_ref)

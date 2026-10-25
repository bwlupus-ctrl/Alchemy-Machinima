"""Compile production LLCamera/frustum and bridge transforms to audit H3.

Only a standalone test is built. Dependencies are installed read-only; all
compiler products go under --out. No viewer build, checkout or GL context.
"""
import argparse
import json
from pathlib import Path
import subprocess

from relight_3d_gl import REPO, SourceTree


def function(text, signature):
    start = text.index(signature)
    end = text.index('\n}', start)+2
    return text[start:end]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source-ref', default='WORKTREE')
    ap.add_argument('--out', default='.tmp/world_seethru/camera')
    ap.add_argument('--dependencies', default='I:/alchemy-machinima/build-Windows-vs2026-os')
    args = ap.parse_args()
    out = Path(args.out).resolve(); out.mkdir(parents=True, exist_ok=True)
    dep = Path(args.dependencies)
    tree = SourceTree(args.source_ref)
    pipe = tree.read('indra/newview/pipeline.cpp')
    cube = pipe[pipe.index('void LLPipeline::generateWorldShadowCubes('):]
    setup = cube[cube.index('            LLCamera shadow_cam = camera;'):]
    setup = setup[:setup.index('\n\n')]
    bridge = tree.read('indra/newview/lldrawable.cpp')
    transform = function(bridge, 'LLCamera LLSpatialBridge::transformCamera(')
    axes = function(tree.read('indra/llmath/llcoordframe.cpp'), 'void LLCoordFrame::setAxes(const LLVector3 &x_axis,')
    assert 'calculateFrustumPlanes' not in axes, 'H3 audit changed: axes now rebuild planes'
    mark = bridge[bridge.index('class LLOctreeMarkNotCulled:'):bridge.index('void LLSpatialBridge::updateDistance(')]
    assert 'AABBInFrustum' not in mark[mark.index('LLCamera trans_camera = transformCamera(camera_in);'):], 'bridge now culls transformed local bounds'
    viewer = function(tree.read('indra/newview/llviewercamera.cpp'), 'void LLViewerCamera::updateFrustumPlanes(')
    preamble = r'''
#include "linden_common.h"
#include "llcamera.h"
#include "m4math.h"
#include "llquaternion.h"
#include "alworldshadowmath.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
using GLint = int;
int gGLViewport[4] = {0,0,544,544};
glm::mat4 current_view, current_proj;
glm::mat4 get_current_modelview() { return current_view; }
glm::mat4 get_current_projection() { return current_proj; }
class LLViewerCamera { public: static void updateFrustumPlanes(LLCamera&,bool,bool,bool); };
// Adapters only for the bridge's owning drawable/xform. The transform body,
// camera type, math, frustum construction and AABB test are production C++.
struct LLXformMatrix {
    LLMatrix4 world;
    LLQuaternion rotation;
    const LLMatrix4& getWorldMatrix() { return world; }
    LLQuaternion getRotation() { return rotation; }
};
struct Drawable { LLXformMatrix mat; LLXformMatrix* getXform() { return &mat; } };
struct LLSpatialBridge { Drawable* mDrawable; LLCamera transformCamera(LLCamera&); };
'''
    body = r'''
int main(int argc,char**) {
    bool axes_control = argc > 1;
    unsigned checks=0;
    for(unsigned face=0;face<6;++face) for(unsigned main_face=0;main_face<6;++main_face) {
        if(axes_control && main_face==face) continue;
        auto a=ALWorldShadow::faceAxis(face), u=ALWorldShadow::faceUp(face);
        auto m=ALWorldShadow::faceAxis(main_face), mu=ALWorldShadow::faceUp(main_face);
        LLVector3 axis(a.x,a.y,a.z), up(u.x,u.y,u.z);
        struct { LLVector3 origin{.7f,-.2f,1.1f}; } job;
        LLCamera camera;
        camera.setAspect(1.7f); camera.setView(.3f);
        camera.setOriginAndLookAt(LLVector3(),LLVector3(mu.x,mu.y,mu.z),LLVector3(m.x,m.y,m.z));
        F32 near_plane=.02f, far_plane=8.f, fov=ALWorldShadow::widenedFovRadians();
        glm::vec3 origin=glm::make_vec3(job.origin.mV);
        current_view=glm::lookAt(origin,origin+glm::vec3(a.x,a.y,a.z),glm::vec3(u.x,u.y,u.z));
        current_proj=glm::perspective(fov,1.f,near_plane,far_plane);
SETUP
        if(axes_control) {
            // Must-fail: the exact hypothesised wrong axes/FOV frustum.
            auto p=shadow_cam.getOrigin(), at=shadow_cam.getAtAxis(), up=shadow_cam.getUpAxis();
            current_view=glm::lookAt(glm::make_vec3(p.mV),glm::make_vec3(p.mV)+glm::make_vec3(at.mV),glm::make_vec3(up.mV));
            current_proj=glm::perspective(shadow_cam.getView(),shadow_cam.getAspect(),near_plane,far_plane);
            LLViewerCamera::updateFrustumPlanes(shadow_cam,false,false,true);
        }
        Drawable drawable; LLSpatialBridge bridge{&drawable};
        LLCamera local=bridge.transformCamera(shadow_cam);
        for(float edge : {0.f,.99f,1.03f}) {
            auto right=ALWorldShadow::faceRight(face);
            LLVector3 centre=job.origin+axis*2.f+LLVector3(right.x,right.y,right.z)*(2.f*edge);
            LLVector4a c,r; c.load3(centre.mV); r.splat(.015f);
            if(!shadow_cam.AABBInFrustum(c,r) || !local.AABBInFrustum(c,r)) {
                std::printf("FAIL avatar bridge face=%u main=%u edge=%.2f\n",face,main_face,edge);return 1;
            }
            ++checks;
        }
        LLVector3 behind=job.origin-axis*2.f;
        LLVector4a c,r;c.load3(behind.mV);r.splat(.015f);
        if(shadow_cam.AABBInFrustum(c,r)) return 2;
    }
    std::printf("PASS %u production cube/bridge AABB checks; opposite-face controls culled\n",checks);
}
'''.replace('SETUP', setup)
    cpp = out/'camera.cpp'; cpp.write_text(preamble+viewer+'\n'+transform+'\n'+body, encoding='utf-8')
    # Compile both relevant production translation units from this tree.
    for name in ('llcamera.cpp', 'llcoordframe.cpp'):
        (out/name).write_text(tree.read('indra/llmath/'+name), encoding='utf-8')
    include = dep/'vcpkg_installed/x64-windows-alchemy/include'
    lib = dep/'vcpkg_installed/x64-windows-alchemy/lib'
    flags = ['/nologo','/std:c++20','/utf-8','/EHsc','/MT','/O2','/Gy','/DNDEBUG','/DLL_WINDOWS=1',
             '/DWIN32','/DNOMINMAX','/DBOOST_ALL_NO_LIB','/DGLM_ENABLE_EXPERIMENTAL',
             '/DLL_PROFILER_CONFIGURATION=1',f'/I"{REPO/"indra/llcommon"}"',
             f'/I"{REPO/"indra/llmath"}"',f'/I"{REPO/"indra/newview"}"',f'/I"{include}"']
    libs = [dep/'llmath/Release/llmath.lib',dep/'llcommon/Release/llcommon.lib']+[p for p in lib.glob('*.lib') if p.name not in ('vlc.lib','vlccore.lib')]
    rsp = out/'compile.rsp'
    rsp.write_text(' '.join(flags+[f'"{cpp}"',f'"{out/"llcamera.cpp"}"',f'"{out/"llcoordframe.cpp"}"',
                                  f'/Fe:"{out/"camera.exe"}"','/link','/OPT:REF']+[f'"{p}"' for p in libs]+
                             ['advapi32.lib','user32.lib','ws2_32.lib','winmm.lib','psapi.lib','bcrypt.lib','crypt32.lib','normaliz.lib',
                              'iphlpapi.lib','shell32.lib','rpcrt4.lib']), encoding='utf-8')
    cmd = out/'compile.cmd'
    cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\ncl @"'+str(rsp)+'"\nexit /b %ERRORLEVEL%\n')
    build = subprocess.run(['cmd','/d','/c',str(cmd)],cwd=out,capture_output=True,text=True)
    (out/'build.log').write_text(build.stdout+build.stderr,encoding='utf-8')
    assert build.returncode == 0, build.stdout+build.stderr
    good = subprocess.run([str(out/'camera.exe')],capture_output=True,text=True)
    bad = subprocess.run([str(out/'camera.exe'),'--axes-control'],capture_output=True,text=True)
    result = dict(source_ref=args.source_ref,passed=good.returncode==0 and bad.returncode!=0,
                  production=good.stdout, must_fail=bad.stdout,axes_rebuild_frustum=False,
                  bridge_local_frustum_cull=False)
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(result)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

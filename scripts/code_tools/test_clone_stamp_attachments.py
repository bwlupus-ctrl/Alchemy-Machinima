#!/usr/bin/env python3
"""Run extracted production stamp tails, attachment move policy and xform propagation.

Standalone C++17 harness; no viewer build. From a VS developer shell:
  python scripts/code_tools/test_clone_stamp_attachments.py
  python scripts/code_tools/test_clone_stamp_attachments.py --revision 48d3abba25c
  python scripts/code_tools/test_clone_stamp_attachments.py --mutate

The old revision and mutation must FAIL with a stale attachment render matrix.
Only viewer services/math types are stubbed; update()/updateMatrix(), reflection
rules, both post-stamp call sequences and the new helper come from production.
The pipeline double defers drawable updates until updateMove(), like the viewer.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def read_source(path, revision):
    if revision:
        return subprocess.run(
            ['git', 'show', f'{revision}:{path}'], cwd=ROOT, check=True,
            capture_output=True, text=True, encoding='utf-8', timeout=15).stdout
    return (ROOT / path).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    # These extracted functions have no braces in string literals/comments.
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def stamp_tail(source, method):
    body = function(source, f'void LLGhostAvatar::{method}(')
    match = re.search(r'mRoot->updateWorldMatrixChildren\(\);\s*'
                      r'(?:moveStampedAttachments\(\);)?', body)
    if not match:
        raise AssertionError(f'{method}: missing final joint propagation')
    return match.group(0)


HARNESS = r'''
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
using F32 = float;
struct LLQuaternion {
    F32 mQ[4];
    LLQuaternion(F32 x=0, F32 y=0, F32 z=0, F32 w=1):mQ{x,y,z,w} {}
};
// LL quaternion composition applies the left operand first.
LLQuaternion operator*(const LLQuaternion& a, const LLQuaternion& b) {
    return {b.mQ[3]*a.mQ[0]+b.mQ[0]*a.mQ[3]+b.mQ[1]*a.mQ[2]-b.mQ[2]*a.mQ[1],
            b.mQ[3]*a.mQ[1]-b.mQ[0]*a.mQ[2]+b.mQ[1]*a.mQ[3]+b.mQ[2]*a.mQ[0],
            b.mQ[3]*a.mQ[2]+b.mQ[0]*a.mQ[1]-b.mQ[1]*a.mQ[0]+b.mQ[2]*a.mQ[3],
            b.mQ[3]*a.mQ[3]-b.mQ[0]*a.mQ[0]-b.mQ[1]*a.mQ[1]-b.mQ[2]*a.mQ[2]};
}
struct LLVector3 {
    F32 mV[3];
    LLVector3(F32 x=0, F32 y=0, F32 z=0):mV{x,y,z} {}
    void scaleVec(const LLVector3& b) { for(int i=0;i<3;++i) mV[i]*=b.mV[i]; }
    LLVector3& operator+=(const LLVector3& b) { for(int i=0;i<3;++i) mV[i]+=b.mV[i]; return *this; }
    LLVector3& operator*=(const LLQuaternion& q) {
        const LLVector3 v=*this, u(q.mQ[0],q.mQ[1],q.mQ[2]);
        const auto cross=[](LLVector3 a,LLVector3 b) {
            return LLVector3(a.mV[1]*b.mV[2]-a.mV[2]*b.mV[1],
                             a.mV[2]*b.mV[0]-a.mV[0]*b.mV[2],
                             a.mV[0]*b.mV[1]-a.mV[1]*b.mV[0]); };
        auto c=cross(u,v), d=cross(u,c);
        for(int i=0;i<3;++i) mV[i]=v.mV[i]+2*(q.mQ[3]*c.mV[i]+d.mV[i]);
        return *this;
    }
};
struct LLMatrix4 {
    F32 mMatrix[4][4]{};
    void initAll(LLVector3 s,LLQuaternion q,LLVector3 p) {
        const F32 x=q.mQ[0],y=q.mQ[1],z=q.mQ[2],w=q.mQ[3];
        *this={};
        mMatrix[0][0]=(1-2*(y*y+z*z))*s.mV[0];
        mMatrix[0][1]=2*(x*y+w*z)*s.mV[0];
        mMatrix[0][2]=2*(x*z-w*y)*s.mV[0];
        mMatrix[1][0]=2*(x*y-w*z)*s.mV[1];
        mMatrix[1][1]=(1-2*(x*x+z*z))*s.mV[1];
        mMatrix[1][2]=2*(y*z+w*x)*s.mV[1];
        mMatrix[2][0]=2*(x*z+w*y)*s.mV[2];
        mMatrix[2][1]=2*(y*z-w*x)*s.mV[2];
        mMatrix[2][2]=(1-2*(x*x+y*y))*s.mV[2];
        for(int i=0;i<3;++i) mMatrix[3][i]=p.mV[i];
        mMatrix[3][3]=1;
    }
};
struct LLXformMatrix {
    LLVector3 mPosition, mWorldPosition, mScale{1,1,1}, mMin, mMax;
    LLQuaternion mRotation, mWorldRotation;
    LLXformMatrix* mParent=nullptr;
    LLMatrix4 mWorldMatrix;
    unsigned mChanged=7;
    static constexpr unsigned MOVED=7;
    bool scaled=false;
    bool getScaleChildOffset()const {return scaled;}
    const LLVector3& getScale()const {return mScale;}
    const LLVector3& getWorldPosition()const {return mWorldPosition;}
    const LLQuaternion& getWorldRotation()const {return mWorldRotation;}
    const LLMatrix4& getWorldMatrix()const {return mWorldMatrix;}
    void setPosition(LLVector3 p) {mPosition=p;}
    void setRotation(LLQuaternion q) {mRotation=q;}
    void setScale(LLVector3 s) {mScale=s;}
    void update();
    void updateMatrix(bool update_bounds=true);
};
// PRODUCTION_XFORM
namespace ALReflect {
// PRODUCTION_REFLECTION
}
template<class T> struct Pointer {
    T* p=nullptr;
    Pointer(T* value=nullptr):p(value){}
    bool isNull()const {return !p;}
    operator T*()const {return p;}
    T* operator->()const {return p;}
};
struct LLSpatialBridge {
    int moves=0;
    unsigned state=0;
    void clearState(unsigned flags) {state &= ~flags;}
};
struct LLDrawable {
    LLXformMatrix mXform;
    LLSpatialBridge* bridge=nullptr;
    bool queued=false;
    static constexpr unsigned EARLY_MOVE=1;
    unsigned state=0;
    int moves=0;
    void clearState(unsigned flags) {state &= ~flags;}
    LLXformMatrix* getXform() {return &mXform;}
    const LLMatrix4& getWorldMatrix()const {return mXform.getWorldMatrix();}
    LLSpatialBridge* getSpatialBridge() {return bridge;}
};
struct LLViewerObject {
    Pointer<LLDrawable> mDrawable;
    bool dead=false, rigged=false, animated=false;
    LLVector3 pos;
    LLQuaternion rot;
    std::vector<LLViewerObject*> children;
    const std::vector<LLViewerObject*>& getChildren()const {return children;}
    bool isDead()const {return dead;}
    bool isRiggedMesh()const {return rigged;}
    bool isAnimatedObject()const {return animated;}
    const LLVector3& getPosition()const {return pos;}
    const LLQuaternion& getRotation()const {return rot;}
};
struct Pipeline {
    std::vector<LLDrawable*> pending;
    void markMoved(LLDrawable* d,bool damped) {
        if(damped) throw std::runtime_error("stamp must move undamped");
        ++d->moves;
        if(!d->queued) {d->queued=true;pending.push_back(d);}
    }
    void markMoved(LLSpatialBridge* b,bool damped) {
        if(damped) throw std::runtime_error("bridge must move undamped");
        ++b->moves;
    }
    void updateMove(const std::vector<LLViewerObject*>& objects) {
        for(auto* d:pending) {
            // pipeline.cpp updateMovedList skips a drawable already moved early.
            for(auto* o:objects) if(!(d->state & LLDrawable::EARLY_MOVE) && o && o->mDrawable.p==d) {
                d->mXform.setPosition(o->pos);
                d->mXform.setRotation(o->rot);
                d->mXform.setScale({1,1,1});
                d->mXform.updateMatrix();
            }
            d->clearState(LLDrawable::EARLY_MOVE);
            d->queued=false;
        }
        pending.clear();
    }
} gPipeline;
struct LLViewerJointAttachment { std::vector<LLViewerObject*> mAttachedObjects; };
struct Root {
    std::vector<LLXformMatrix*> joints;
    void updateWorldMatrixChildren() {for(auto* j:joints) j->updateMatrix();}
};
struct LLGhostAvatar {
    Root* mRoot=nullptr;
    std::map<int,LLViewerJointAttachment*> mAttachmentPoints;
    void moveStampedAttachments();
    void reflectTail() { // PRODUCTION_REFLECT_TAIL
    }
    void trueMirrorTail() { // PRODUCTION_TRUE_MIRROR_TAIL
    }
};
// PRODUCTION_HELPER
void require(bool ok,const char* msg) {if(!ok) throw std::runtime_error(msg);}
bool equal(const LLMatrix4& a,const LLMatrix4& b) {
    for(int i=0;i<4;++i) for(int j=0;j<4;++j)
        if(std::fabs(a.mMatrix[i][j]-b.mMatrix[i][j])>2.e-5f) return false;
    return true;
}
// Independent homogeneous composition oracle: local offset * joint world.
LLMatrix4 expected(const LLViewerObject& o,const LLXformMatrix& joint) {
    LLMatrix4 local,result;
    local.initAll({1,1,1},o.rot,o.pos);
    const auto& parent=joint.getWorldMatrix();
    for(int i=0;i<4;++i) for(int j=0;j<4;++j) for(int k=0;k<4;++k)
        result.mMatrix[i][j]+=local.mMatrix[i][k]*parent.mMatrix[k][j];
    return result;
}
LLQuaternion turn(F32 radians,int axis) {
    LLQuaternion q; q.mQ[axis]=std::sin(radians*.5f);q.mQ[3]=std::cos(radians*.5f);return q;
}
void run(bool reflect) {
    gPipeline={};
    LLXformMatrix head, mouth;
    head.setPosition({3,4,2});
    mouth.mParent=&head; mouth.setPosition({.1f,.07f,.03f});
    Root skeleton{{&head,&mouth}};
    LLDrawable drawable,rigged_drawable,animesh_drawable,dead_drawable;
    LLSpatialBridge bridge;
    drawable.bridge=&bridge;
    LLViewerObject cigarette,rigged,animesh,dead,loading;
    cigarette.mDrawable=&drawable; cigarette.pos={.12f,.02f,-.03f}; cigarette.rot=turn(.4f,0);
    rigged.mDrawable=&rigged_drawable;rigged.rigged=true;
    animesh.mDrawable=&animesh_drawable;animesh.animated=true;
    dead.mDrawable=&dead_drawable;dead.dead=true;
    std::vector<LLViewerObject*> objects{&cigarette,&rigged,&animesh,&dead,&loading,nullptr};
    LLViewerJointAttachment point{objects};
    LLGhostAvatar ghost{&skeleton,{{1,&point},{2,nullptr}}};
    for(auto* d:{&drawable,&rigged_drawable,&animesh_drawable,&dead_drawable}) d->mXform.mParent=&mouth;
    const auto stamp=[&]() {if(reflect) ghost.reflectTail(); else ghost.trueMirrorTail();};
    for(int frame=0;frame<40;++frame) {
        // Alternate whether an incidental drawable update captures the clone's
        // paused pose before the source stamp: the pre-fix visible alternation.
        head.setRotation(turn(-.8f,2)); skeleton.updateWorldMatrixChildren();
        if(frame%2==0) {
            drawable.mXform.setPosition(cigarette.pos);drawable.mXform.setRotation(cigarette.rot);
            drawable.mXform.updateMatrix();
            drawable.state |= LLDrawable::EARLY_MOVE;
            bridge.state |= LLDrawable::EARLY_MOVE; // idleUpdateMisc() early bridge move
        }
        LLVector3 p(3+.01f*frame,.2f+.03f*frame,2+.02f*frame);
        LLQuaternion q=turn(.1f+.03f*frame,2)*turn(.01f*frame,0);
        head.setPosition(reflect?ALReflect::vector(p):p);
        head.setRotation(reflect?ALReflect::rotation(q):q);
        stamp();
        require(!(bridge.state & LLDrawable::EARLY_MOVE),
                "bridge EARLY_MOVE left set: bridge bounds would keep the pre-stamp pose");
        gPipeline.updateMove(objects);
        require(equal(drawable.getWorldMatrix(),expected(cigarette,mouth)),
                "stale attachment render matrix after stamp (moving source/alternating paused pose)");
    }
    require(drawable.moves==40 && bridge.moves==40,"missing root or bridge move");
    require(!rigged_drawable.moves && !animesh_drawable.moves && !dead_drawable.moves,
            "rigged/animesh/dead attachment was moved");
    for(int frame=0;frame<40;++frame) {stamp();gPipeline.updateMove(objects);}
    require(drawable.moves==40 && bridge.moves==40,"static clone queues redundant moves");
    // Same stamped source pose, but a foreign writer dirties the drawable cache.
    drawable.mXform.mWorldMatrix.mMatrix[3][0]+=1;
    stamp();gPipeline.updateMove(objects);
    require(equal(drawable.getWorldMatrix(),expected(cigarette,mouth)),"unchanged stamp failed to repair cache");
    // Attachment offset edit and LOD/skin classification changes must use current state.
    cigarette.pos={-.3f,.4f,.1f};cigarette.rot=turn(.7f,1);
    stamp();gPipeline.updateMove(objects);
    require(equal(drawable.getWorldMatrix(),expected(cigarette,mouth)),"edited offset ignored");
    cigarette.rigged=true;head.setRotation(turn(.9f,0));
    const int saved_moves=drawable.moves;
    stamp();gPipeline.updateMove(objects);
    require(drawable.moves==saved_moves,"new rigged classification ignored");
    cigarette.rigged=false;stamp();gPipeline.updateMove(objects);
    require(equal(drawable.getWorldMatrix(),expected(cigarette,mouth)),"unrigged LOD transition stale");
    LLDrawable replacement;replacement.mXform.mParent=&mouth;cigarette.mDrawable=&replacement;
    stamp();gPipeline.updateMove(objects);
    require(equal(replacement.getWorldMatrix(),expected(cigarette,mouth)),"replacement drawable stale");
    require(!rigged_drawable.moves && !animesh_drawable.moves,"exclusions changed");
    // Mixed linkset: a rigged root with a rigid child renders the child through
    // the root's world matrix, so the root must follow the stamp.
    LLViewerObject rigid_child; rigid_child.mDrawable=nullptr;
    rigged.children.push_back(&rigid_child);
    head.setRotation(turn(-.5f,1));
    stamp();gPipeline.updateMove(objects);
    require(rigged_drawable.moves==1,"rigged root with a rigid child was not moved (mixed linkset)");
    require(equal(rigged_drawable.getWorldMatrix(),expected(rigged,mouth)),"mixed linkset root stale");
    rigged.children.clear();
}
int main() {
    int failures=0;
    for(bool reflect:{true,false}) {
        const char* mode=reflect?"Reflect":"True Mirror";
        try {run(reflect);std::cout<<"PASS: "<<mode<<": 40 moving + 40 static frames, early moves, cache repair, offset edit, exclusions, LOD, replacement\n";}
        catch(const std::exception& e) {++failures;std::cerr<<"FAIL: "<<mode<<": "<<e.what()<<"\n";}
    }
    return failures?1:0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='cl.exe')
    parser.add_argument('--revision')
    parser.add_argument('--mutate', action='store_true', help='remove post-stamp root move')
    args = parser.parse_args()
    ghost = read_source('indra/newview/llghostavatar.cpp', args.revision)
    xform = read_source('indra/llmath/xform.cpp', args.revision)
    reflection = read_source('indra/newview/alreflectmath.h', args.revision)
    signature = 'void LLGhostAvatar::moveStampedAttachments()'
    helper = function(ghost, signature) if signature in ghost else signature + ' {}'
    if args.mutate:
        original = helper
        helper = helper.replace('gPipeline.markMoved(drawable, false);', '')
        assert original != helper, 'mutation did not remove the root move'
    generated = HARNESS
    replacements = {
        '// PRODUCTION_XFORM': '\n'.join(function(xform, signature) for signature in
            ('void LLXformMatrix::update()', 'void LLXformMatrix::updateMatrix(')),
        '// PRODUCTION_REFLECTION': '\n'.join(function(reflection, signature) for signature in
            ('inline LLQuaternion rotation(', 'inline LLVector3 vector(')),
        '// PRODUCTION_HELPER': helper,
        '// PRODUCTION_REFLECT_TAIL': stamp_tail(ghost, 'applyReflectStamp'),
        '// PRODUCTION_TRUE_MIRROR_TAIL': stamp_tail(ghost, 'applyTrueMirrorStamp'),
    }
    for marker, code in replacements.items():
        generated = generated.replace(marker, code)
    # Integration guard: helpers are reached only from the two self-gated stamps.
    if signature in ghost:
        assert ghost.count('moveStampedAttachments();') == 2
        for method, mode in [('applyReflectStamp', 'DRIVE_REFLECT'),
                             ('applyTrueMirrorStamp', 'DRIVE_TRUE_MIRROR')]:
            body = function(ghost, f'void LLGhostAvatar::{method}(')
            assert f'mEntityDriveMode != ALGhostStudio::{mode}' in body
            assert body.index('moveStampedAttachments();') > body.index('mRoot->updateWorldMatrixChildren();')
    with tempfile.TemporaryDirectory(prefix='clone_stamp_') as scratch:
        scratch = Path(scratch)
        source = scratch / 'test.cpp'
        source.write_text(generated, encoding='utf-8')
        exe = scratch / 'test.exe'
        subprocess.run([args.compiler, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX',
                        str(source), '/Fe:' + str(exe), '/Fo:' + str(scratch / 'test.obj')],
                       check=True, cwd=scratch, timeout=60)
        return subprocess.run([str(exe)], check=False, timeout=15).returncode


if __name__ == '__main__':
    raise SystemExit(main())

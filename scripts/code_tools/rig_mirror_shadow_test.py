"""Compile/run the production shadow auction with lightweight object stubs.

Run from a VS developer prompt: python scripts/code_tools/rig_mirror_shadow_test.py
No viewer build or renderer changes. Extract both the mirror policy and auction
so reverting the policy reproduces the recipient-order shadow regression.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / ".tmp" / "rig_mirror_shadow_test"


def between(text, start, end):
    return text.split(start, 1)[1].split(end, 1)[0]


def main():
    rig = (ROOT / "indra/newview/alcinelightrig.cpp").read_text()
    mirror = between(rig, "void ALCineLightRig::mirrorFrom(",
                     "void ALCineLightRig::setAnchor(")
    policy = between(mirror, "else LLPipeline::clearGoboOverride(to);",
                     "F32 softness = 0.f;")
    pipeline = (ROOT / "indra/newview/pipeline.cpp").read_text()
    shadow_assignment = pipeline.split(
        '// [BDMerge G3.3 Batch 3] Per-projector "cast shadows" opt-out:', 1)[1]
    auction = "if (!gCubeSnapshot && !sPrismLensRender &&" + between(
        shadow_assignment, "if (!gCubeSnapshot && !sPrismLensRender &&",
        "// make sure we didn't end up targeting the same spot light")
    suppressed = between(pipeline,
        "bool LLPipeline::isProjectorShadowSuppressed(LLVOVolume* volume)\n{",
        "// [BDMerge G3.3 Batch 1 C]").rsplit("}", 1)[0]
    source = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <set>
using S32 = int; using U32 = unsigned; using F32 = float;
#define llassert assert
struct LLVOVolume {
    int id, bits; bool rig; float priority;
    int getID() const { return id; }
    bool isCineRigEmitter() const { return rig; }
    float getSpotLightPriority() const { return priority; }
    LLVOVolume* getRootEdit() { return this; }
};
using LLViewerObject = LLVOVolume;
struct LLDrawable { LLVOVolume* v; LLVOVolume* getVOVolume() { return v; } };
struct Ptr {
    LLDrawable* p = nullptr;
    Ptr& operator=(LLDrawable* q) { p = q; return *this; }
    operator LLDrawable*() const { return p; }
    LLDrawable* operator->() const { return p; }
    bool notNull() const { return p; }
};
LLVOVolume lights[] = {{1,3,true,4}, {2,3,true,4},
                       {3,2,true,4}, {4,2,true,1000},
                       {5,0,false,5}, {6,0,true,3}};
struct ALCineLightRigManager {
    static bool anyActorOnly() { return true; }
    static ALCineLightRigManager& instance() { static ALCineLightRigManager m; return m; }
    int actorOnlyBits(int id) { return lights[id-1].bits; }
};
struct LLPipeline {
    static std::set<int> sNoShadowProjectors;
    static bool isProjectorNoShadow(int id) { return sNoShadowProjectors.count(id); }
    static void toggleProjectorCastShadows(int id) {
        if (!sNoShadowProjectors.erase(id)) sNoShadowProjectors.insert(id);
    }
    static bool isProjectorShadowSuppressed(LLVOVolume* volume) {
''' + suppressed + r'''
    }
};
std::set<int> LLPipeline::sNoShadowProjectors;
bool gCubeSnapshot = false, sPrismLensRender = false, mActorMaskValid = true;
enum class ActorMaskTestMode { F1, F3 }; enum class ActorMaskView { MASK };
ActorMaskTestMode mActorMaskTestMode = ActorMaskTestMode::F1;
ActorMaskView mActorMaskView = ActorMaskView::MASK;
int mMainRT; int* mRT = &mMainRT; unsigned slots = 2;
Ptr mTargetShadowSpotLight[10];
unsigned bdmergeMaxSpotShadows() { return slots; }
bool isProjectorShadowSuppressed(LLVOVolume* v) {
    return LLPipeline::isProjectorShadowSuppressed(v);
}
void assign(LLDrawable* drawablep) {
    auto* volume = drawablep->getVOVolume();
''' + auction + r'''
}
void mirrorPolicy(int from, int to) {
''' + policy + r'''
}
int main() {
    // Copy suppression is independent of the source's cast-shadow setting.
    for (int sourceSuppressed : {0,1}) {
        LLPipeline::sNoShadowProjectors.clear();
        if (sourceSuppressed) LLPipeline::sNoShadowProjectors.insert(1);
        const bool before = LLPipeline::isProjectorNoShadow(1);
        mirrorPolicy(1,3); mirrorPolicy(1,4); mirrorPolicy(1,3);
        assert(LLPipeline::isProjectorNoShadow(1) == before);
        for (unsigned budget : {2u,4u,10u}) {
            slots = budget;
            std::array<int,6> order = {0,1,2,3,4,5};
            do {
                LLDrawable draw[6];
                for (int i=0; i<6; ++i) draw[i].v = &lights[i];
                for (auto& p : mTargetShadowSpotLight) p = nullptr;
                for (int i : order) if (i != 2 && i != 3) assign(&draw[i]);
                std::array<LLDrawable*,10> baseline{};
                for (int i=0; i<10; ++i) baseline[i] = mTargetShadowSpotLight[i];
                for (auto& p : mTargetShadowSpotLight) p = nullptr;
                for (int i : order) assign(&draw[i]);
                for (int i=0; i<10; ++i)
                    assert(baseline[i] == mTargetShadowSpotLight[i]);
            } while (std::next_permutation(order.begin(),order.end()));
        }
    }
    std::cout << "PASS: identical original shadow assignments in 4320 auctions\n";
}
'''
    OUT.mkdir(parents=True, exist_ok=True)
    cpp = OUT / "shadow_test.cpp"
    exe = OUT / "shadow_test.exe"
    cpp.write_text(source)
    subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", "/W3",
                    str(cpp), f"/Fe{exe}", f"/Fo{OUT / 'shadow_test.obj'}"], check=True)
    subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Compile production nearby-light update/selection loops against small fixtures.

No viewer build. Requires MSVC (discovered on Windows) or c++ on PATH.
Run with PYTHONUTF8=1; build artifacts stay in .tmp/alpha_port/nearby_lights.
Distance, renderer and drawable dependencies are fixtures; the set comparator,
update loop, hardware-light filtering, six-slot selection and fade clock are
extracted verbatim from pipeline.h/cpp on every run.
"""

from pathlib import Path
import os
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.tmp/alpha_port/nearby_lights'


def braced(source, start):
    """Return through the matching closing brace (these blocks have no brace literals)."""
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


FIXTURES = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <utility>
#include <vector>
using F32 = float;
using S32 = int;
using U32 = unsigned;
U32 gFrameCount = 0, mNearbyLightFadeFrame = ~0U;
template<class T> T llmin(T a, T b) { return std::min(a,b); }
template<class T> using LLPointer = T*;
struct LLUUID {
    int id = 0;
    static const LLUUID null;
    bool operator!=(const LLUUID& b) const { return id != b.id; }
    bool operator<(const LLUUID& b) const { return id < b.id; }
};
const LLUUID LLUUID::null{};
struct LLVOAvatar {
    bool complex = false, muted = false, slow = false;
    bool isTooComplex() const { return complex; }
    bool isInMuteList() const { return muted; }
    bool isTooSlow() const { return slow; }
};
LLVOAvatar* gAgentAvatarp = nullptr;
struct LLViewerObject {
    bool attached = false, rig = false;
    LLVOAvatar* avatar = nullptr;
    bool isAttachment() const { return attached; }
    bool isCineRigEmitter() const { return rig; }
    LLVOAvatar* getAvatar() const { return avatar; }
};
struct LLColor3 {
    F32 mV[3] = {1, 1, 1};
    LLColor3 operator*(F32 f) const {
        LLColor3 result = *this;
        result *= f;
        return result;
    }
    void operator*=(F32 f) { for (auto& v : mV) v *= f; }
    F32 magVecSquared() const { return mV[0]*mV[0]+mV[1]*mV[1]+mV[2]*mV[2]; }
};
struct LLColor4 {
    F32 mV[4] = {1, 1, 1, 0};
    LLColor4(const LLColor3& c) : mV{c.mV[0], c.mV[1], c.mV[2], 1} {}
    LLColor4 operator*(F32 f) const {
        LLColor4 result = *this;
        result *= f;
        return result;
    }
    void operator*=(F32 f) { for (auto& v : mV) v *= f; }
    F32 magVecSquared() const { return mV[0]*mV[0]+mV[1]*mV[1]+mV[2]*mV[2]; }
};
struct LLVOVolume : LLViewerObject {
    F32 position = 0;
    int id = 0;
    int getID() const { return id; }
    LLColor3 getLightLinearColor() const { return {}; }
    F32 getLightRadius() const { return 1; }
};
struct LLDrawable {
    enum { NEARBY_LIGHT = 1, LIGHT = 2, ACTIVE = 4 };
    LLVOVolume volume;
    bool valid_volume = true;
    int state = NEARBY_LIGHT | LIGHT;
    LLVOVolume* getVOVolume() { return valid_volume ? &volume : nullptr; }
    const LLViewerObject* getVObj() const { return &volume; }
    bool isState(int bit) const { return (state & bit) != 0; }
    void clearState(int bit) { state &= ~bit; }
};
constexpr F32 LIGHT_FADE_TIME = 1;
bool sRenderAttachedLights = true, world_enabled = true, attached_enabled = true;
bool bdmerge_should_render_light(bool attached, bool) {
    return attached ? attached_enabled : world_enabled;
}
F32 calc_light_dist(LLVOVolume* light, F32 origin, F32) {
    return std::abs(light->position - origin);
}
template<class T> T llclamp(T v, T lo, T hi) { return std::clamp(v, lo, hi); }
struct Interval { F32 value() const { return 0.25f; } } gFrameIntervalSeconds;
struct ALCineLightRigManager {
    static ALCineLightRigManager& instance() { static ALCineLightRigManager m; return m; }
    int actorOnlyBits(int) const { return 0; }
};
struct ALEnvIntensity { static F32 localLightEVScale(LLVOVolume*) { return 1; } };
struct ReflectionManager {
    bool isCinematicLiveProbeIgnoredLight(int) const { return false; }
    bool isCinematicLiveProbePinnedLight(int) const { return false; }
    bool isCinematicLiveProbeCapture() const { return false; }
} mReflectionMapManager;
'''

TESTS = r'''
const Light& entry(LLDrawable* drawable) {
    auto it = std::find_if(mNearbyLights.begin(), mNearbyLights.end(),
        [=](const Light& l) { return l.drawable == drawable; });
    assert(it != mNearbyLights.end());
    return *it;
}
int main() {
    std::array<LLDrawable, 8> lights;
    std::map<LLDrawable*, const Light*> addresses;
    for (int i = 0; i < 8; ++i) {
        lights[i].volume.position = F32(i);
        lights[i].volume.id = i;
        auto it = mNearbyLights.insert(Light(&lights[i], F32(i), 1, LLUUID{i+1})).first;
        addresses[&lights[i]] = &*it;
    }
    // Independently rank by fixture geometry after each camera/avatar movement.
    for (F32 origin : {7.f, 3.25f, 0.f, 5.75f}) {
        update(origin, 100);
        std::vector<LLDrawable*> expected;
        for (auto& d : lights) expected.push_back(&d);
        std::sort(expected.begin(), expected.end(), [=](auto a, auto b) {
            return std::abs(a->volume.position-origin) < std::abs(b->volume.position-origin);
        });
        auto it = mNearbyLights.begin();
        for (auto* d : expected) {
            assert(it->drawable == d);
            assert(it->dist == std::abs(d->volume.position-origin));
            assert(&*it == addresses[d]); // original set node, no replacement allocation
            assert(it->areaFeedID.id == d->volume.id+1); // retain fork metadata
            ++it;
        }
        auto hardware = setup();
        assert(hardware.size() == 6);
        assert(std::equal(hardware.begin(), hardware.end(), expected.begin()));
    }
    std::cout << "PASS current-distance ordering, original nodes/metadata, current nearest six HW lights\n";

    mNearbyLights.clear();
    auto& d = lights[0];
    d.volume.position = 1;
    mNearbyLights.insert(Light(&d, 1, 1));
    d.volume.position = 30; // an existing light moves outside the range
    update(0, 10);
    assert(entry(&d).fade < 0 && entry(&d).dist == 30);
    F32 previous = entry(&d).fade;
    for (int frame = 0; frame < 4; ++frame) {
        setup();
        update(0, 10);
        if (mNearbyLights.empty()) break;
        assert(entry(&d).fade < previous);
        previous = entry(&d).fade;
    }
    update(0, 10);
    assert(mNearbyLights.empty());
    assert(!d.isState(LLDrawable::NEARBY_LIGHT));
    std::cout << "PASS out-of-range fade clock reaches expiry, erases node and clears NEARBY_LIGHT\n";

    // Six retained closer lights must not starve a re-sorted seventh fade.
    mNearbyLights.clear();
    for (int i = 0; i < 7; ++i) {
        lights[i] = LLDrawable{};
        lights[i].volume.position = F32(i + 1);
        mNearbyLights.insert(Light(&lights[i], F32(i + 1), 1));
    }
    lights[6].volume.position = 30;
    update(0, 10);
    for (int frame = 0; frame < 5; ++frame) {
        assert(setup().size() == 6);
        update(0, 10);
    }
    assert(mNearbyLights.size() == 6);
    assert(!lights[6].isState(LLDrawable::NEARBY_LIGHT));
    std::cout << "PASS seventh out-of-range light expires behind six retained HW lights\n";

    mNearbyLights.clear();

    d.state |= LLDrawable::NEARBY_LIGHT;
    d.volume.position = 30;
    mNearbyLights.insert(Light(&d, 30, .75f));
    update(0, 10);
    assert(entry(&d).fade == -.5f);
    const F32 unchanged = entry(&d).fade;
    setup();
    setup();
    update(0, 10, false);
    assert(entry(&d).fade == unchanged); // no setup/view can double-step a frame
    d.volume.position = 1;
    update(0, 10);
    assert(entry(&d).fade == .75f);
    std::cout << "PASS partial fade reverses continuously when a light returns\n";

    // Consecutive rejected nodes expose both double-increment and missing-erase bugs.
    for (int reason = 0; reason < 8; ++reason) {
        mNearbyLights.clear();
        world_enabled = attached_enabled = sRenderAttachedLights = true;
        LLVOAvatar avatar;
        for (int i = 0; i < 8; ++i) {
            lights[i] = LLDrawable{};
            lights[i].volume.position = F32(i);
            lights[i].volume.rig = i >= 6;
            mNearbyLights.insert(Light(&lights[i], F32(i), 1));
        }
        for (int i = 0; i < 6; ++i) {
            auto& rejected = lights[i];
            rejected.volume.attached = reason < 5;
            rejected.volume.avatar = &avatar;
            if (reason == 0) sRenderAttachedLights = false;
            if (reason == 1) attached_enabled = false;
            if (reason == 2) avatar.complex = true;
            if (reason == 3) avatar.muted = true;
            if (reason == 4) avatar.slow = true;
            if (reason == 5) world_enabled = false;
            if (reason == 6) rejected.valid_volume = false;
            if (reason == 7) rejected.clearState(LLDrawable::LIGHT);
        }
        update(0, 100);
        assert(mNearbyLights.size() == 2);
        assert(mNearbyLights.begin()->drawable == &lights[6]);
        assert(mNearbyLights.rbegin()->drawable == &lights[7]);
        for (int i = 0; i < 6; ++i) assert(!lights[i].isState(LLDrawable::NEARBY_LIGHT));
    }
    std::cout << "PASS adjacent erasures for all eight upstream/fork rejection paths\n";
}
'''


def main():
    source = (ROOT / 'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    header = (ROOT / 'indra/newview/pipeline.h').read_text(encoding='utf-8')
    light = braced(header, header.index('struct Light\n')) + ';'
    update = source.split('// UPDATE THE EXISTING NEARBY LIGHTS', 1)[1].split(
        '// FIND NEW LIGHTS THAT ARE IN RANGE', 1)[0]
    # Also constrain the allocation contract; runtime checks verify node addresses.
    assert 'mNearbyLights.extract(iter++)' in update
    assert 'updated_lights.insert(std::move(node))' in update
    assert 'mNearbyLights.swap(updated_lights)' in update
    assert 'insert(Light(' not in update
    setup_source = source[source.index('void LLPipeline::setupHWLights()'):]
    selection_start = setup_source.index('if (effective_local_light_count >= 1)')
    selection = braced(setup_source, selection_start)
    # Verify the real main-view call site updates before forward-light selection.
    assert 'calcNearbyLights(camera);\n    setupHWLights();' in source
    selection_color = braced(source, source.index('static LLColor3 local_light_selection_color('))
    program = FIXTURES + selection_color + '\n' + light + r'''
using light_set_t = std::set<Light, Light::compare>;
light_set_t mNearbyLights;
void update(F32 cam_pos, F32 max_dist, bool next_frame = true) {
    if (next_frame) ++gFrameCount;
''' + update + r'''
}
std::vector<LLDrawable*> setup() {
    int effective_local_light_count = 256, cur_light = 2, mLightMovingMask = 0;
    bool actor_only = false, actor_as_world = false, cinematic_pinned_only = false;
    bool sRenderDeferred = true;
    F32 light_scale = 1;
    std::vector<LLDrawable*> selected;
    auto write_local_light = [&](LLVOVolume* light, LLColor4, F32, int) {
        for (const auto& l : mNearbyLights)
            if (l.drawable->getVOVolume() == light) selected.push_back(l.drawable);
    };
''' + selection + '\nreturn selected;\n}\n' + TESTS
    OUT.mkdir(parents=True, exist_ok=True)
    cpp = OUT / 'nearby_lights.cpp'
    cpp.write_text(program, encoding='utf-8')
    env = os.environ.copy()
    if os.name == 'nt':
        if not shutil.which('cl'):
            candidates = sorted(Path(env.get('ProgramFiles', 'C:/Program Files')).glob(
                'Microsoft Visual Studio/*/*/VC/Auxiliary/Build/vcvars64.bat'))
            if not candidates:
                raise RuntimeError('MSVC vcvars64.bat not found')
            result = subprocess.run(
                f'cmd /d /s /c ""{candidates[-1]}" >nul && set"',
                check=True, capture_output=True, text=True)
            env.update(line.split('=', 1) for line in result.stdout.splitlines()
                       if '=' in line and not line.startswith('='))
        exe = OUT / 'nearby_lights.exe'
        compiler = shutil.which('cl', path=env.get('Path', env.get('PATH')))
        if not compiler:
            raise RuntimeError('cl.exe not found after loading MSVC environment')
        command = [compiler, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX',
                   str(cpp), '/Fe:' + str(exe), '/Fo:' + str(OUT / 'nearby_lights.obj')]
    else:
        exe = OUT / 'nearby_lights'
        command = ['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(exe)]
    subprocess.run(command, env=env, cwd=OUT, check=True)
    subprocess.run([str(exe)], env=env, cwd=OUT, check=True)


if __name__ == '__main__':
    main()

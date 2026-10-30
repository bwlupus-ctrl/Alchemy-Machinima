/**
 * @file alprobereach_test.cpp
 * @brief Reach boundaries, secondary views and allocation hysteresis.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "../test/lltut.h"
#include "../alprobereach.h"

#include <fstream>
#include <limits>
#include <sstream>

namespace tut
{
struct probe_reach_data
{
    F32 camera[3] = { 128.f, 128.f, 30.f };

    static std::string source(const char* file)
    {
        const std::string self(__FILE__);
        const auto slash = self.find_last_of("/\\");
        std::ifstream input(self.substr(0, slash) + "/../" + file, std::ios::binary);
        ensure("production source is readable", input.good());
        std::ostringstream text;
        text << input.rdbuf();
        return text.str();
    }
};
typedef test_group<probe_reach_data> probe_reach_group;
typedef probe_reach_group::object probe_reach_object;
probe_reach_group probe_reach_tests("alprobereach");

template<> template<> void probe_reach_object::test<1>()
{
    const F32 tangent[3] = { 272.f, 128.f, 30.f };
    const F32 beyond[3] = { 273.f, 128.f, 30.f };
    const F32 diagonal[3] = { 238.f, 238.f, 30.f };
    const F32 above[3] = { 128.f, 128.f, 230.f };
    const F32 large[3] = { 428.f, 128.f, 30.f };
    ensure("touching draw distance stays", ALProbeReach::inReach(tangent, 16.f, camera, 128.f));
    ensure("wholly beyond draw distance leaves", !ALProbeReach::inReach(beyond, 16.f, camera, 128.f));
    ensure("all three axes matter", !ALProbeReach::inReach(diagonal, 16.f, camera, 128.f));
    ensure("vertical distance matters", !ALProbeReach::inReach(above, 16.f, camera, 128.f));
    ensure("large placed sphere intersects", ALProbeReach::inReach(large, 180.f, camera, 128.f));
    ensure("inside sphere with negative draw", ALProbeReach::inReach(camera, 16.f, camera, -10.f));
    ensure("negative draw clamps to zero", !ALProbeReach::inReach(beyond, 16.f, camera, -10.f));

    for (U32 x = 0; x < 8; ++x)
    {
        for (U32 y = 0; y < 8; ++y)
        {
            const F32 departed[3] = { 2176.f + x * 32.f, 128.f + y * 32.f, 20.f };
            ensure("departed region grid is irrelevant", !ALProbeReach::inReach(departed, 24.f, camera, 256.f));
        }
    }
}

template<> template<> void probe_reach_object::test<2>()
{
    const F32 remote[3] = { 2000.f, 128.f, 30.f };
    const F32 near_remote[3] = { 2070.f, 128.f, 30.f };
    ALProbeReach::Views views;
    views.add(camera, 128.f);
    ensure("main eye alone cannot reach remote probe", !views.inReach(near_remote, 8.f));
    views.add(remote, 64.f);
    ensure("VCam's draw distance keeps remote probe", views.inReach(near_remote, 8.f));
    const F32 outside_both[3] = { 2073.f, 128.f, 30.f };
    ensure("outside every view leaves", !views.inReach(outside_both, 8.f));
    ensure("default/live/Director exemption keeps far probe", views.inReach(outside_both, 8.f, true));
    views.mKeepAll = true;
    ensure("unbounded secondary/local-probe view fails open", views.inReach(outside_both, 8.f));
}

template<> template<> void probe_reach_object::test<3>()
{
    const F32 nan = std::numeric_limits<F32>::quiet_NaN();
    const F32 inf = std::numeric_limits<F32>::infinity();
    F32 invalid[3] = { nan, 0.f, 0.f };
    ensure("NaN check survives fast math", !ALProbeReach::finite(nan));
    ensure("infinity check survives fast math", !ALProbeReach::finite(inf));
    ensure("invalid origin retains", ALProbeReach::inReach(invalid, 1.f, camera, 128.f));
    ensure("invalid eye retains", ALProbeReach::inReach(camera, 1.f, invalid, 128.f));
    ensure("invalid radius retains", ALProbeReach::inReach(camera, nan, camera, 128.f));
    ensure("negative radius retains", ALProbeReach::inReach(camera, -1.f, camera, 128.f));
    ensure("invalid range retains", ALProbeReach::inReach(camera, 1.f, camera, inf));
    ALProbeReach::Views views;
    ensure("missing eyes retain", views.inReach(camera, 1.f));
    for (U32 i = 0; i < 10; ++i)
    {
        views.add(camera, 1.f);
    }
    ensure("overflow cannot lose a view", views.mKeepAll);
    const F32 extreme[3] = { std::numeric_limits<F32>::max(), 0.f, 0.f };
    ensure("squared distance does not overflow in float", !ALProbeReach::inReach(extreme, 1.f, camera, 128.f));
}

template<> template<> void probe_reach_object::test<4>()
{
    ensure_equals("empty initial count", ALProbeReach::stepRelevant(0, 0), 0u);
    ensure_equals("default included in first step", ALProbeReach::stepRelevant(1, 0), 16u);
    ensure_equals("exact step", ALProbeReach::stepRelevant(16, 0), 16u);
    ensure_equals("up immediately", ALProbeReach::stepRelevant(33, 32), 48u);
    U32 count = 48;
    for (U32 i = 0; i < 32; ++i)
    {
        count = ALProbeReach::stepRelevant(i % 2 ? 33 : 32, count);
        ensure_equals("crossing an edge cannot flap allocation", count, 48u);
    }
    ensure_equals("one lower step holds", ALProbeReach::stepRelevant(17, 48), 48u);
    ensure_equals("rounded lower step holds", ALProbeReach::stepRelevant(31, 48), 48u);
    ensure_equals("more than a step lower shrinks", ALProbeReach::stepRelevant(16, 48), 16u);
    ensure_equals("large upward jump is immediate", ALProbeReach::stepRelevant(250, 16), 256u);
}

template<> template<> void probe_reach_object::test<5>()
{
    ensure_equals("dynamic default is disabled", ALProbeReach::dynamicProbeCount(1, -1, 16, 256), 256u);
    ensure_equals("level zero", ALProbeReach::dynamicProbeCount(0, 0, 48, 256), 1u);
    ensure_equals("level one uses relevant count", ALProbeReach::dynamicProbeCount(1, 0, 48, 256), 48u);
    ensure_equals("single spare cube", ALProbeReach::dynamicProbeCount(1, 1, 48, 256), 49u);
    ensure_equals("empty still gets default cube", ALProbeReach::dynamicProbeCount(1, 0, 0, 256), 1u);
    ensure_equals("level two floor", ALProbeReach::dynamicProbeCount(2, 0, 48, 256), 128u);
    ensure_equals("level two above floor", ALProbeReach::dynamicProbeCount(2, 0, 160, 256), 160u);
    ensure_equals("level three", ALProbeReach::dynamicProbeCount(3, 0, 48, 256), 256u);
    ensure_equals("existing rounding down", ALProbeReach::dynamicProbeCount(1, 16, 39, 256), 32u);
    ensure_equals("existing rounding up", ALProbeReach::dynamicProbeCount(1, 16, 40, 256), 48u);
    ensure_equals("configured cap", ALProbeReach::dynamicProbeCount(1, 0, 4000, 128), 128u);
}

// The numeric policy is executable above. These seams additionally catch losing
// the multi-view/exemption wiring in the GL-dependent manager, without a viewer.
template<> template<> void probe_reach_object::test<6>()
{
    const auto manager = source("llreflectionmapmanager.cpp");
    const auto probe = source("llreflectionmap.cpp");
    const auto feeds = source("llprismlens.cpp");
    const auto refresh = probe.find("gPipeline.mReflectionMapManager.updateProbeReach();");
    const auto reject = probe.find("if (!mInReach)");
    ensure("manager prepares reach before rejecting relevance", refresh < reject && reject != std::string::npos);
    ensure("old behaviour has a real branch", probe.find("if (reach_cull)") < refresh);
    ensure("one pass per frame", manager.find("if (mReachFrame == gFrameCount)") != std::string::npos);
    ensure("secondary union is wired", manager.find("LLPrismLens::extendProbeReach(views);") != std::string::npos);
    ensure("main camera included", manager.find("views.add(camera.mV, gAgentCamera.mDrawDistance);") != std::string::npos);
    ensure("live exemption wired", manager.find("probe == mCinematicLiveProbe ||") != std::string::npos);
    ensure("Director local exemption wired", manager.find("probe->mViewerObject->isLocalOnly()") != std::string::npos);
    ensure("virtual VCam eye/range wired", feeds.find("views.add(camera.mVirtualPos.mV, camera.mFarClip);") != std::string::npos);
    ensure("object VCam eye/range wired", feeds.find("views.add(eye.mV, camera.mFarClip);") != std::string::npos);
    ensure("allocation uses stepped previous count", manager.find("mRenderReflectionProbeDynamicAllocation, mRelevantProbeCount,") != std::string::npos);
    ensure("current count is stepped", manager.find("ALProbeReach::stepRelevant(mReachRelevantCount, mRelevantProbeCount)") != std::string::npos);
}
}

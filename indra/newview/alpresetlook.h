#ifndef AL_PRESET_LOOK_H
#define AL_PRESET_LOOK_H

#include "alsettingsundohistory.h"
#include "alkithistory.h"
#include "llsd.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

class ALKitHistory;

// A preset replaces its entire owned look. Validate the selection/file first,
// then open one Load, reset owned controls, and apply the authored values.
// Save the same owned set. Ownership excludes targeting, shot transforms,
// calibration/preferences, quality/performance, shader permutations and master
// enables. Renderer-resolved selectors and optional blocks can remain sparse.
namespace ALPresetLook
{
enum Family { RIM, ROTO, TRON, INFRARED, SOFT_EDGE, HAZE, FLARE, NIGHT_MASK, LIGHT_RIG,
              GRAD_ND, POLARIZER, LENS_FILTER, SHAKE, WEIGHTED, DRONE, RIM_CUSTOM, TRON_PALETTE };
const std::vector<std::string>& owned(Family family);
const std::vector<std::string>& infraredGrainSettings();
std::vector<std::string> fixtureSettings(const std::string& prefix);
std::vector<std::string> pageSettings(const std::string& page);
void reset(const std::vector<std::string>& settings);
LLSD snapshot(const std::vector<std::string>& settings);
void apply(const std::vector<std::string>& settings, const LLSD& values);
bool replaying();

class Load
{
public:
    explicit Load(const std::string& label = "Apply preset");
    ~Load();
    void recordState(const std::string& key, std::function<bool(bool, std::string&)> replay,
                     std::function<bool(bool, std::string&)> validate);
    Load(const Load&) = delete;
    Load& operator=(const Load&) = delete;
private:
    std::unique_ptr<ALKitHistory::ScopedSuspend> mSuspend;
    std::unique_ptr<ALExposureSettingsHistory::ScopedBatch> mBatch;
    ALKitHistory* mHistory = nullptr;
};
}
#endif

/** Session-only, local prim overrides. No simulator writes or scene/settings data.
 * Server snapshots are in object parent coordinates, never inferred from a driven pose.
 * Build selection and grab gestures pause a drive; Track remains armed.
 */
#ifndef AL_PROJECTOR_TRACKER_H
#define AL_PROJECTOR_TRACKER_H
#include "alprojectortrackmath.h"
#include "lluuid.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
class LLViewerObject;
class LLViewerRegion;
class ALProjectorTracker
{
public:
    enum Mode { AIM, FOLLOW };
    struct State
    {
        LLUUID subject;
        bool hasSubject = false;
        S32 joint = 0;
        Mode mode = AIM;
        F32 smoothing = .15f;
        bool enabled = false;
        std::string status;
        ALProjectorTrackMath::Pose server, current, offset;
        LLUUID parent;
        LLViewerRegion* region = nullptr;
        F64 missingSince = 0.;
        bool captured = false;
        bool paused = false;
        bool overridden = false;
        bool restorePending = false;
    };
    static ALProjectorTracker& instance();
    const State& state(const LLUUID& id) const;
    void configure(const LLUUID& id, const LLUUID& subject, bool has_subject,
                   S32 joint, Mode mode, F32 smoothing);
    void enable(const LLUUID& id, bool enabled);
    void capture(const LLUUID& id);
    void update(F32 dt);
    void clear();
    bool tracked(const LLUUID& id) const { return mActive.count(id) != 0; }
    U64 revision() const { return mRevision; }
    // Remove the override before partial packets; snapshot decoded transforms
    // only after packet ordering validation (including script-driven moves).
    void beforeObjectUpdate(LLViewerObject* object);
    void receivedServerPose(LLViewerObject* object, const LLVector3& position, const LLQuaternion& rotation);
    void drop(const LLUUID& id);
    // Returns true if any selected linkset drive was restored and paused.
    bool pauseSelectionForEdit();
    bool pauseObjectForEdit(LLViewerObject* object);
    void selectionChanged();
    bool selectionSendBarrier();
    void beginGrab(LLViewerObject* object);
    void endGrab();
    bool canUseTransform(LLViewerObject* object) const;
    static bool eligible(LLViewerObject* object);
    // Read-only Build display: never seed a spinner or clipboard with a fake
    // tracked transform, even if Track is switched off before the next commit.
    bool serverEditTransform(LLViewerObject* object, ALProjectorTrackMath::Pose& pose) const;
    static const char* jointName(S32 index);
private:
    void stop(const LLUUID& id, const std::string& status);
    bool restore(LLViewerObject* object, State& state);
    bool pause(LLViewerObject* object, State& state, const char* status);
    bool selected(LLViewerObject* object) const;
    bool grabbed(LLViewerObject* object) const;
    void refreshPause(LLViewerObject* object, State& state);
    std::unordered_map<LLUUID, State> mStates;
    std::unordered_set<LLUUID> mActive;
    LLUUID mGrabRoot;
    U64 mRevision = 1;
    LLViewerRegion* mRegion = nullptr;
};
#endif

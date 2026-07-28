# Flycam Recorder — Deep-Research Brief (with full source)

**Audience:** an LLM doing deep research (e.g. ChatGPT). Everything needed to reason
about the system is inline — architecture, the complete current source, the settings,
and the integration seams — followed by the research questions.

**The viewer:** Alchemy-Machinima, a fork of the Alchemy Second Life viewer (itself a
descendant of the Linden Lab / Firestorm codebase) tuned for **machinima** — in-world
film-making. It is a large single-process C++ OpenGL client with a Windows-style
message pump (`LLAppViewer::idle()` runs once per frame). UI is XUI (XML) + C++ panels;
persistent settings live in `gSavedSettings` (`app_settings/settings.xml`).

---

## 0. What I want from this research

Analyze the camera-path recorder below and propose a **prioritized, concretely-scoped
roadmap** to evolve it into a professional machinima camera-animation tool. Cover, with
specific implementation approaches that fit the architecture described here:

1. **Correctness / robustness** gaps in recording, interpolation, and anchoring.
2. **Keyframe editing UX** — today there is *no* in-viewer keyframe editing (you record,
   scrub, and hand-edit the XML). What is the right in-viewer timeline / dope-sheet and
   in-world path-editing model, given the constraints below?
3. **Interpolation & motion quality** — constant-speed playback, per-key easing/tangents,
   arc-length reparameterization, focus/DoF keyframing.
4. **Integration** with the viewer's other camera and *time* systems (see §5), especially
   the brand-new **Temporal Capture / World Time Scale** presentation clock and a
   deterministic **fixed-frame capture** path for rendering clean video.
5. **File-format / interop** — round-tripping camera animation with pro tools (Blender,
   After Effects, Maya, Nuke `.chan`, glTF, FBX camera).

For each recommendation, give: the concrete change, where it hooks into the code below,
the data-model impact (the take format is an LLSD document), risk, and rough effort.
Prefer a phased plan (ship-small-first). Call out anything that is architecturally
impossible or a bad idea in this engine.

---

## 1. Concept

The Flycam Recorder records the **final render camera** — position, full orientation,
and vertical FOV — once per frame at a configurable sample rate, **regardless of what is
driving the camera** (joystick "flycam", mouse orbit, the Cinematic Camera, etc.).
Playback **re-takes** the camera (a branch in the idle camera dispatch) and evaluates the
recorded path with linear or smooth-spline interpolation, so even sparse hand-edited
keyframes produce fluid motion. Takes save/load as a single hand-editable LLSD-XML
document. Playback supports speed scaling, loop / ping-pong / hold, pause, scrubbing, an
optional procedural **handheld-operator** texture layer, and **relative playback**
(re-anchor a recorded move onto a different avatar/object/camera).

It is exposed as a shared panel (`ALPanelFlycamRecorder`) embedded in **both** a
standalone floater and the **Director Console → Takes tab**, driving one global
`LLFlycamRecorder` singleton.

---

## 2. Architecture at a glance

```
RECORD                                   PLAYBACK
------                                   --------
idle():                                  idle() camera dispatch:
  camera dispatch runs (flycam/orbit/      if FlycamRecorder.isPlaybackActive():
    cinematic/... writes LLViewerCamera)       FlycamRecorder.updateCamera()   // re-takes the camera
  FlycamRecorder.onIdleFrame():                  advance playhead by wall dt * speed
    if RECORDING and enough time passed:         evalPose(playhead) -> pos/rot/fov (lerp or spline)
      sampleCamera(t)                            applyAnchor(pos,rot)          // relative playback
        capture LLViewerCamera origin/           optional handheld-operator layer
        quaternion/FOV as a Keyframe             write LLViewerCamera
```

- **Time domain:** recording uses a wall-clock `LLTimer`; playback advances the playhead
  by `gFrameIntervalSeconds` (the viewer's wall frame delta) × a speed multiplier. The
  recorder is deliberately its **own** time domain — see §5 (it is *not* currently
  affected by the new world-time scaling).
- **Coordinate space:** keyframes store camera origin in **global** coordinates
  (`LLVector3d`, double precision — the SL world is a 256 m-gridded plane that can be far
  from the region origin), orientation as a world-frame `LLQuaternion`, FOV in radians.
- **Interpolation:** position via uniform Catmull-Rom, orientation via **squad**
  (spherical cubic; the raw quaternion log/exp math is hand-rolled because
  `LLQuaternion`'s component ctor normalizes and would destroy the pure log quaternions),
  FOV via linear. Linear mode (`nlerp` + `lerp`) is available.
- **Anchoring (relative playback):** a take records the recording avatar's pose (the
  "record anchor"); on playback the path's offsets from that anchor are mapped onto a
  live anchor (self / selection / camera-at-play), **yaw-only** so the horizon stays
  level. Optional per-frame "follow" rides a walking subject.
- **Persistence:** LLSD-XML, `version` currently 2 (v2 added the anchor frame).

---

## 3. Engine — `llflycamrecorder.h`

```cpp
class LLFlycamRecorder
{
public:
    enum EState { STATE_IDLE=0, STATE_RECORDING=1, STATE_PLAYING=2, STATE_PAUSED=3 };
    enum ELoopMode { LOOP_HOLD_END=0, LOOP_REPEAT=1, LOOP_PINGPONG=2 };
    enum EAnchorMode { ANCHOR_WORLD=0, ANCHOR_SELF=1, ANCHOR_SELECTION=2, ANCHOR_CAMERA=3 };

    struct Keyframe
    {
        F32          mTime = 0.f;   // seconds from take start
        LLVector3d   mPosGlobal;    // camera origin, global coordinates
        LLQuaternion mRot;          // camera orientation, world frame
        F32          mFov = 1.f;    // vertical FOV, radians
    };

    static LLFlycamRecorder& instance();

    // frame hooks (called from the idle loop)
    bool isPlaybackActive() const;
    void updateCamera();   // advance playback and write the render camera
    void onIdleFrame();    // sample the render camera if recording

    // transport
    void startRecording(); void stopRecording();
    void startPlayback();  void pausePlayback(); void togglePlayback();
    void stopPlayback();   void clear();

    // scrubbing
    void seek(F32 time);

    // file I/O (LLSD XML)
    bool saveToFile(const std::string& filename);
    bool loadFromFile(const std::string& filename);

    // introspection
    EState getState() const; F32 getPlayhead() const; F32 getDuration() const;
    S32 getNumKeyframes() const; const std::string& getStatus() const;

private:
    LLFlycamRecorder() = default;
    void sampleCamera(F32 time);
    void evalPose(F32 time, LLVector3d& pos, LLQuaternion& rot, F32& fov) const;
    void canonicalizeRotations();
    void latchRecordAnchor();
    bool resolveLiveAnchor(LLVector3d& pos, F32& yaw);
    void applyAnchor(LLVector3d& pos, LLQuaternion& rot);

    EState                mState = STATE_IDLE;
    std::vector<Keyframe> mKeys;
    F32                   mPlayhead = 0.f;
    F32                   mPlayDir = 1.f;     // ping-pong direction
    LLTimer               mRecordTimer;
    std::string           mStatus;
    // recorded anchor frame (saved with the take)
    bool mHaveRecAnchor = false; LLVector3d mRecAnchorPos; F32 mRecAnchorYaw = 0.f;
    // live anchor, latched at playback start (or followed per-frame)
    bool mHaveLiveAnchor = false; S32 mLiveAnchorMode = -1;
    LLVector3d mLiveAnchorPos; F32 mLiveAnchorYaw = 0.f;
    // previous playback pose, for feeding the handheld operator
    bool mHavePrev = false; LLVector3 mPrevPosAgent; LLQuaternion mPrevRot;
};
```

---

## 4. Engine — `llflycamrecorder.cpp` (complete)

```cpp
namespace
{
constexpr S32 TAKE_FORMAT_VERSION = 2;  // 2: added anchor_pos/anchor_yaw

// Raw quaternion math for squad. LLQuaternion's component ctor normalizes its
// input, which destroys the pure (w=0) log quaternions squad needs.
struct FRQuat { F32 x, y, z, w; };
inline FRQuat fr_from(const LLQuaternion& q) { return FRQuat{q.mQ[VX],q.mQ[VY],q.mQ[VZ],q.mQ[VW]}; }
inline LLQuaternion fr_to(const FRQuat& q)
{
    LLQuaternion out; out.mQ[VX]=q.x; out.mQ[VY]=q.y; out.mQ[VZ]=q.z; out.mQ[VW]=q.w;
    out.normQuat(); return out;
}
inline FRQuat fr_mul(const FRQuat& a, const FRQuat& b)
{
    return FRQuat{ a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
                   a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
                   a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
                   a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z }; }
inline FRQuat fr_conj(const FRQuat& q)          { return FRQuat{-q.x,-q.y,-q.z,q.w}; }
inline FRQuat fr_scale(const FRQuat& q, F32 s)  { return FRQuat{q.x*s,q.y*s,q.z*s,q.w*s}; }
inline FRQuat fr_add(const FRQuat& a, const FRQuat& b){ return FRQuat{a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w}; }

FRQuat fr_log(const FRQuat& q)   // log of a unit quaternion -> pure quaternion (w=0)
{
    const F32 w = llclamp(q.w,-1.f,1.f);
    const F32 s = sqrtf(llmax(0.f, 1.f - w*w));
    if (s < 1e-6f) return FRQuat{0.f,0.f,0.f,0.f};
    const F32 k = acosf(w) / s;
    return FRQuat{ q.x*k, q.y*k, q.z*k, 0.f };
}
FRQuat fr_exp(const FRQuat& q)   // exp of a pure quaternion -> unit quaternion
{
    const F32 theta = sqrtf(q.x*q.x + q.y*q.y + q.z*q.z);
    if (theta < 1e-6f) return FRQuat{0.f,0.f,0.f,1.f};
    const F32 k = sinf(theta)/theta;
    return FRQuat{ q.x*k, q.y*k, q.z*k, cosf(theta) };
}
FRQuat fr_slerp(const FRQuat& a, FRQuat b, F32 t)
{
    F32 dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    if (dot < 0.f) { b = fr_scale(b,-1.f); dot = -dot; }
    if (dot > 0.9995f) {   // nearly parallel: normalized lerp
        FRQuat r = fr_add(fr_scale(a,1.f-t), fr_scale(b,t));
        const F32 m = sqrtf(r.x*r.x+r.y*r.y+r.z*r.z+r.w*r.w);
        return (m>1e-8f)? fr_scale(r,1.f/m) : a;
    }
    const F32 theta = acosf(llclamp(dot,-1.f,1.f));
    const F32 inv_sin = 1.f/sinf(theta);
    return fr_add(fr_scale(a, sinf((1.f-t)*theta)*inv_sin),
                  fr_scale(b, sinf(t*theta)*inv_sin));
}
FRQuat fr_squadInner(const FRQuat& qm1, const FRQuat& q0, const FRQuat& qp1)
{
    const FRQuat inv = fr_conj(q0);
    const FRQuat l0 = fr_log(fr_mul(inv, qm1));
    const FRQuat l1 = fr_log(fr_mul(inv, qp1));
    return fr_mul(q0, fr_exp(fr_scale(fr_add(l0,l1), -0.25f)));
}
FRQuat fr_squad(const FRQuat& q1, const FRQuat& a, const FRQuat& b, const FRQuat& q2, F32 t)
{ return fr_slerp(fr_slerp(q1,q2,t), fr_slerp(a,b,t), 2.f*t*(1.f-t)); }

F32 fr_yawOf(const LLQuaternion& q)   // heading (yaw about world Z) of the at-axis
{ LLMatrix3 m(q); const LLVector3 at(m.mMatrix[0]); return atan2f(at.mV[VY], at.mV[VX]); }

// uniform Catmull-Rom
inline F64 fr_catmullRom(F64 p0,F64 p1,F64 p2,F64 p3,F64 u)
{
    return 0.5*((2.0*p1) + (-p0+p2)*u + (2.0*p0-5.0*p1+4.0*p2-p3)*u*u + (-p0+3.0*p1-3.0*p2+p3)*u*u*u);
}
inline LLVector3d fr_catmullRom(const LLVector3d& p0,const LLVector3d& p1,const LLVector3d& p2,const LLVector3d& p3,F64 u)
{
    return LLVector3d(fr_catmullRom(p0.mdV[VX],p1.mdV[VX],p2.mdV[VX],p3.mdV[VX],u),
                      fr_catmullRom(p0.mdV[VY],p1.mdV[VY],p2.mdV[VY],p3.mdV[VY],u),
                      fr_catmullRom(p0.mdV[VZ],p1.mdV[VZ],p2.mdV[VZ],p3.mdV[VZ],u));
}
} // anonymous namespace

LLFlycamRecorder& LLFlycamRecorder::instance() { static LLFlycamRecorder s; return s; }

bool LLFlycamRecorder::isPlaybackActive() const
{ return (mState==STATE_PLAYING || mState==STATE_PAUSED) && !mKeys.empty(); }

F32 LLFlycamRecorder::getDuration() const { return mKeys.empty()? 0.f : mKeys.back().mTime; }

// ---- transport ----
void LLFlycamRecorder::startRecording()
{
    if (mState==STATE_RECORDING) return;
    mState=STATE_RECORDING; mKeys.clear(); mPlayhead=0.f; mPlayDir=1.f;
    mHavePrev=false; mHaveLiveAnchor=false; mRecordTimer.reset();
    sampleCamera(0.f); latchRecordAnchor(); mStatus="Recording...";
}
void LLFlycamRecorder::stopRecording()
{
    if (mState!=STATE_RECORDING) return;
    sampleCamera(mRecordTimer.getElapsedTimeF32());   // final pose
    mState=STATE_IDLE; mStatus=llformat("Recorded %.1f s (%d keys)", getDuration(), getNumKeyframes());
}
void LLFlycamRecorder::startPlayback()
{
    if (mState==STATE_RECORDING) stopRecording();
    if (mKeys.empty()) { mStatus="Nothing to play"; return; }
    if (mPlayhead >= getDuration()) mPlayhead=0.f;    // replay from top when parked at end
    mPlayDir=1.f; mHavePrev=false; mHaveLiveAnchor=false;   // re-latch anchor fresh
    LLCameraOperator::instance().reset();
    mState=STATE_PLAYING; mStatus="Playing";
}
void LLFlycamRecorder::pausePlayback(){ if (mState==STATE_PLAYING){ mState=STATE_PAUSED; mStatus="Paused"; } }
void LLFlycamRecorder::togglePlayback(){ if (mState==STATE_PLAYING) pausePlayback(); else startPlayback(); }
void LLFlycamRecorder::stopPlayback()
{ if (mState==STATE_PLAYING||mState==STATE_PAUSED){ mState=STATE_IDLE; mPlayhead=0.f; mPlayDir=1.f; mStatus="Stopped"; } }
void LLFlycamRecorder::clear()
{
    if (mState==STATE_RECORDING || isPlaybackActive()) mState=STATE_IDLE;
    mKeys.clear(); mPlayhead=0.f; mPlayDir=1.f; mHaveRecAnchor=false; mHaveLiveAnchor=false; mStatus="Cleared";
}
void LLFlycamRecorder::seek(F32 time)
{
    if (mKeys.empty()) return;
    mPlayhead = llclamp(time, 0.f, getDuration());
    mHavePrev = false;                        // don't let the operator see the jump as motion
    if (mState==STATE_IDLE){ mState=STATE_PAUSED; mStatus="Paused"; }   // preview from idle
}

// ---- recording ----
void LLFlycamRecorder::onIdleFrame()
{
    if (mState!=STATE_RECORDING) return;
    static LLCachedControl<F32> sample_rate(gSavedSettings, "FlycamRecSampleRate", 30.f);
    const F32 interval = 1.f / llclamp((F32)sample_rate, 1.f, 120.f);
    const F32 t = mRecordTimer.getElapsedTimeF32();
    if (mKeys.empty() || t - mKeys.back().mTime >= interval) sampleCamera(t);
}
void LLFlycamRecorder::sampleCamera(F32 time)
{
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    Keyframe key;
    key.mTime = time;
    key.mPosGlobal = gAgent.getPosGlobalFromAgent(cam->getOrigin());
    key.mRot = cam->getQuaternion();
    key.mFov = cam->getView();
    if (!mKeys.empty())
    {
        const LLQuaternion& prev = mKeys.back().mRot;   // same-hemisphere for interpolation
        const F32 dot = prev.mQ[VX]*key.mRot.mQ[VX]+prev.mQ[VY]*key.mRot.mQ[VY]
                      + prev.mQ[VZ]*key.mRot.mQ[VZ]+prev.mQ[VW]*key.mRot.mQ[VW];
        if (dot < 0.f) key.mRot = -key.mRot;
        if (time <= mKeys.back().mTime) return;          // guard duplicate timestamps
    }
    mKeys.push_back(key);
}
void LLFlycamRecorder::canonicalizeRotations()
{
    for (size_t i=1;i<mKeys.size();++i){
        const LLQuaternion& prev=mKeys[i-1].mRot; LLQuaternion& cur=mKeys[i].mRot;
        const F32 dot = prev.mQ[VX]*cur.mQ[VX]+prev.mQ[VY]*cur.mQ[VY]+prev.mQ[VZ]*cur.mQ[VZ]+prev.mQ[VW]*cur.mQ[VW];
        if (dot<0.f) cur=-cur;
    }
}

// ---- anchoring (relative playback) ----
void LLFlycamRecorder::latchRecordAnchor()
{
    if (isAgentAvatarValid()){
        mRecAnchorPos = gAgentAvatarp->getPositionGlobal();
        mRecAnchorYaw = fr_yawOf(gAgentAvatarp->getRenderRotation());
        mHaveRecAnchor = true;
    } else if (!mKeys.empty()){
        mRecAnchorPos = mKeys[0].mPosGlobal;
        mRecAnchorYaw = fr_yawOf(mKeys[0].mRot);
        mHaveRecAnchor = true;
    }
}
bool LLFlycamRecorder::resolveLiveAnchor(LLVector3d& pos, F32& yaw)
{
    static LLCachedControl<S32>  anchor_mode(gSavedSettings, "FlycamRecAnchorMode", 0);
    static LLCachedControl<bool> follow(gSavedSettings, "FlycamRecFollowAnchor", false);
    const S32 mode = anchor_mode;
    if (mode <= ANCHOR_WORLD || mode > ANCHOR_CAMERA) return false;
    if (mHaveLiveAnchor && mLiveAnchorMode != mode) mHaveLiveAnchor = false;   // mode switch invalidates latch

    const bool want_fresh = !mHaveLiveAnchor || (follow && mode != ANCHOR_CAMERA);
    if (want_fresh)
    {
        bool resolved=false; LLVector3d a_pos; F32 a_yaw=0.f;
        if (mode==ANCHOR_CAMERA){
            LLViewerCamera* cam = LLViewerCamera::getInstance();
            a_pos = gAgent.getPosGlobalFromAgent(cam->getOrigin());
            a_yaw = fr_yawOf(cam->getQuaternion()); resolved=true;
        } else {
            LLVOAvatar* av=nullptr; LLViewerObject* root=nullptr;
            if (mode==ANCHOR_SELECTION){
                if (LLViewerObject* obj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject()){
                    av = obj->getAvatar(); if (!av) root = obj->getRootEdit();
                }
            }
            if (!av && !root && isAgentAvatarValid()) av = (LLVOAvatar*)gAgentAvatarp;   // SELF / fallback
            if (av && !av->isDead()){ a_pos=av->getPositionGlobal(); a_yaw=fr_yawOf(av->getRenderRotation()); resolved=true; }
            else if (root && !root->isDead()){ a_pos=root->getPositionGlobal(); a_yaw=fr_yawOf(root->getRenderRotation()); resolved=true; }
        }
        if (resolved){ mLiveAnchorPos=a_pos; mLiveAnchorYaw=a_yaw; mLiveAnchorMode=mode; mHaveLiveAnchor=true; }
        // resolution failure keeps the previous latch (anchor left mid-shot) so the camera doesn't snap
    }
    if (!mHaveLiveAnchor) return false;
    pos = mLiveAnchorPos; yaw = mLiveAnchorYaw; return true;
}
void LLFlycamRecorder::applyAnchor(LLVector3d& pos, LLQuaternion& rot)
{
    static LLCachedControl<S32> anchor_mode(gSavedSettings, "FlycamRecAnchorMode", 0);
    LLVector3d live_pos; F32 live_yaw;
    if (!resolveLiveAnchor(live_pos, live_yaw) || mKeys.empty()) return;

    LLVector3d ref_pos; F32 ref_yaw;   // frame the recorded pose is expressed against
    if ((S32)anchor_mode==ANCHOR_CAMERA || !mHaveRecAnchor){ ref_pos=mKeys[0].mPosGlobal; ref_yaw=fr_yawOf(mKeys[0].mRot); }
    else { ref_pos=mRecAnchorPos; ref_yaw=mRecAnchorYaw; }

    const F32 dyaw = live_yaw - ref_yaw;                          // yaw-only keeps recorded roll/pitch (level horizon)
    const LLQuaternion rz(dyaw, LLVector3(0.f,0.f,1.f));
    LLVector3 offset((F32)(pos.mdV[VX]-ref_pos.mdV[VX]),(F32)(pos.mdV[VY]-ref_pos.mdV[VY]),(F32)(pos.mdV[VZ]-ref_pos.mdV[VZ]));
    offset = offset * rz;
    pos = live_pos + LLVector3d(offset);
    rot = rot * rz;
}

// ---- playback ----
void LLFlycamRecorder::evalPose(F32 time, LLVector3d& pos, LLQuaternion& rot, F32& fov) const
{
    static LLCachedControl<bool> smooth(gSavedSettings, "FlycamRecSmooth", true);
    const size_t n = mKeys.size();
    if (n==1){ pos=mKeys[0].mPosGlobal; rot=mKeys[0].mRot; fov=mKeys[0].mFov; return; }

    size_t i2=1; while (i2<n-1 && mKeys[i2].mTime<time) ++i2;     // bracketing segment [i1,i2]
    const size_t i1=i2-1; const Keyframe& k1=mKeys[i1]; const Keyframe& k2=mKeys[i2];
    const F32 span = k2.mTime-k1.mTime;
    const F32 u = (span>1e-6f)? llclamp((time-k1.mTime)/span,0.f,1.f) : 0.f;
    fov = lerp(k1.mFov, k2.mFov, u);
    if (!smooth){ pos=lerp(k1.mPosGlobal,k2.mPosGlobal,(F64)u); rot=nlerp(u,k1.mRot,k2.mRot); return; }

    const Keyframe& k0=mKeys[(i1>0)?i1-1:0]; const Keyframe& k3=mKeys[(i2+1<n)?i2+1:n-1];   // clamped neighbors
    pos = fr_catmullRom(k0.mPosGlobal,k1.mPosGlobal,k2.mPosGlobal,k3.mPosGlobal,(F64)u);
    const FRQuat q0=fr_from(k0.mRot),q1=fr_from(k1.mRot),q2=fr_from(k2.mRot),q3=fr_from(k3.mRot);
    const FRQuat a=fr_squadInner(q0,q1,q2), b=fr_squadInner(q1,q2,q3);
    rot = fr_to(fr_squad(q1,a,b,q2,u));
}
void LLFlycamRecorder::updateCamera()
{
    static LLCachedControl<F32>  speed(gSavedSettings, "FlycamRecSpeed", 1.f);
    static LLCachedControl<S32>  loop_mode(gSavedSettings, "FlycamRecLoopMode", 0);
    static LLCachedControl<bool> use_operator(gSavedSettings, "FlycamRecUseOperator", false);
    if (mKeys.empty()) return;

    const F32 dt = llclamp(gFrameIntervalSeconds.value(), 0.0005f, 0.25f);
    const F32 duration = getDuration();
    if (mState==STATE_PLAYING)
    {
        mPlayhead += dt * llclamp((F32)speed,0.05f,5.f) * mPlayDir;
        if (mPlayhead>=duration || mPlayhead<=0.f)
        {
            switch ((S32)loop_mode){
                case LOOP_REPEAT: mPlayhead=(mPlayhead>=duration)?mPlayhead-duration:mPlayhead+duration; mHavePrev=false; break;
                case LOOP_PINGPONG: mPlayhead=(mPlayhead>=duration)?2.f*duration-mPlayhead:-mPlayhead; mPlayDir=-mPlayDir; break;
                default: mPlayhead=llclamp(mPlayhead,0.f,duration); mState=STATE_PAUSED; mStatus="Finished (holding last frame)"; break;
            }
        }
        mPlayhead = llclamp(mPlayhead,0.f,duration);
    }

    LLVector3d pos_global; LLQuaternion rot; F32 fov;
    evalPose(mPlayhead, pos_global, rot, fov);
    applyAnchor(pos_global, rot);

    LLVector3 out_pos = gAgent.getPosAgentFromGlobal(pos_global);
    LLQuaternion out_rot = rot; F32 fov_mul = 1.f;

    if (use_operator && mState==STATE_PLAYING)   // optional handheld texture (same idiom as LLCinematicCamera)
    {
        if (mHavePrev){
            LLMatrix3 axes(out_rot);
            const LLVector3 world_vel = (out_pos - mPrevPosAgent) * (1.f/dt);
            LLQuaternion dq = out_rot * ~mPrevRot;
            F32 d_roll,d_pitch,d_yaw; LLMatrix3(dq).getEulerAngles(&d_roll,&d_pitch,&d_yaw);
            LLCameraOperatorInput opin;
            opin.mDeltaTime = dt;
            opin.mLinearVel = LLVector3(world_vel*LLVector3(axes.mMatrix[0]), world_vel*LLVector3(axes.mMatrix[1]), world_vel*LLVector3(axes.mMatrix[2]));
            opin.mAngularVel = LLVector3(d_roll,d_pitch,d_yaw) * (1.f/dt);
            const LLCameraOperatorOutput op = LLCameraOperator::instance().update(opin);
            LLMatrix3 wobble(op.mRoll,op.mPitch,op.mYaw);
            mPrevPosAgent=out_pos; mPrevRot=out_rot;
            out_rot = LLQuaternion(wobble)*out_rot;
            LLMatrix3 out_axes(out_rot);
            out_pos += LLVector3(out_axes.mMatrix[0])*op.mPosOffset.mV[VX]
                     + LLVector3(out_axes.mMatrix[1])*op.mPosOffset.mV[VY]
                     + LLVector3(out_axes.mMatrix[2])*op.mPosOffset.mV[VZ];
            fov_mul = op.mFovMul;
        } else { mPrevPosAgent=out_pos; mPrevRot=out_rot; mHavePrev=true; }
    }

    LLViewerCamera* cam = LLViewerCamera::getInstance();
    LLMatrix3 final_axes(out_rot);
    cam->setView(fov*fov_mul);
    cam->setOrigin(out_pos);
    cam->mXAxis=LLVector3(final_axes.mMatrix[0]); cam->mYAxis=LLVector3(final_axes.mMatrix[1]); cam->mZAxis=LLVector3(final_axes.mMatrix[2]);
}
```

**File I/O** (LLSD-XML; abbreviated — save writes `{version, region, anchor_pos,
anchor_yaw, keyframes:[{t,pos,rot,fov}]}`; load tolerates hand-edited files by sorting on
`t`, dropping duplicate timestamps, normalizing quaternions, and deriving the anchor from
the first keyframe when absent):

```cpp
bool LLFlycamRecorder::saveToFile(const std::string& filename)
{
    if (mKeys.empty()){ mStatus="Nothing to save"; return false; }
    LLSD doc;
    doc["version"] = TAKE_FORMAT_VERSION;
    if (LLViewerRegion* r = gAgent.getRegion()) doc["region"] = r->getName();
    if (mHaveRecAnchor){ doc["anchor_pos"]=ll_sd_from_vector3d(mRecAnchorPos); doc["anchor_yaw"]=(LLSD::Real)mRecAnchorYaw; }
    LLSD keys = LLSD::emptyArray();
    for (const Keyframe& k : mKeys){
        LLSD rec; rec["t"]=(LLSD::Real)k.mTime; rec["pos"]=ll_sd_from_vector3d(k.mPosGlobal);
        rec["rot"]=ll_sd_from_quaternion(k.mRot); rec["fov"]=(LLSD::Real)k.mFov; keys.append(rec);
    }
    doc["keyframes"] = keys;
    llofstream file(filename.c_str());
    if (!file.is_open()){ mStatus="Couldn't write "+filename; return false; }
    LLSDSerialize::toPrettyXML(doc, file); file.close();
    mStatus=llformat("Saved %d keys", getNumKeyframes()); return true;
}
// loadFromFile: parse LLSD, read keyframes[], std::stable_sort by t, dedupe equal t,
// normQuat each, canonicalizeRotations(), read anchor_pos/anchor_yaw or derive from key[0].
```

---

## 5. Where it plugs in

### 5.1 Idle camera dispatch (`llappviewer.cpp`, once per frame)

Playback is one branch in a priority chain of camera drivers; recording samples the final
camera *after* whatever drove it:

```cpp
    if (gAgentPilot.isPlaying() && ...) { gAgentPilot.moveCamera(); }
    else if (LLFlycamRecorder::instance().isPlaybackActive())
    {
        LLFlycamRecorder::instance().updateCamera();   // recorded path playback / scrubbing
    }
    else if (LLPathCamera::instance().isActive()) { /* per-node path cameras */ }
    else if (/* cinematic camera active */) { /* LLCinematicCamera */ }
    else { gAgentCamera.updateCamera(); }              // normal camera
    ...
    // after the camera is final for the frame:
    LLFlycamRecorder::instance().onIdleFrame();          // sample into the recorder if RECORDING
```

### 5.2 Sibling systems (context for integration questions)

- **`LLViewerCamera`** — the single render camera the recorder reads (record) and writes
  (playback): origin (agent space), X/Y/Z axes, vertical FOV.
- **`LLCameraOperator`** — a procedural *handheld* operator (breathing, sway, drift, lag)
  that the recorder can layer on top of playback, fed velocities derived from the path.
- **`LLCinematicCamera`** — pattern/tripod/orbit/dolly modes framing "Subject A/B"; the
  operator layer uses the same idiom.
- **Actor Pathing (`ALToolPathEdit` / `ALPanelPathEditor`)** — an in-world node editor for
  *actor* walk paths: click-to-place nodes, drag to move, screen-space node picking,
  Catmull-Rom evaluation, per-node markers drawn in-world. **This is the closest existing
  precedent for an in-world *camera*-path editor.**
- **Director Console** — an ACTION/CUT "clapperboard": arming checkboxes let ACTION start
  recorder **playback** of the loaded take *or* a fresh recorder **capture**; a "scene"
  save/load persists cast, subjects, and settings-backed parameters together.
- **NEW — Temporal Capture / World Time Scale (`LLPresentationTime`)** — a just-added
  client-side *presentation clock* that scales all local cinematic dynamics (avatar
  animation, particles, texture animation, object spin) from 0×(freeze) to 8×, while the
  grid connection stays real-time. **The Flycam Recorder is deliberately its own time
  domain and is NOT currently driven by this clock** (the design left "play a take on the
  world clock" and "fixed-frame capture" as future work).

### 5.3 Settings (`gSavedSettings`, defaults from the code)

| Key | Type | Default | Meaning |
|---|---|---|---|
| `FlycamRecSampleRate` | F32 | 30 | keyframes captured per second while recording (1–120) |
| `FlycamRecSpeed` | F32 | 1.0 | playback speed multiplier (0.05–5) |
| `FlycamRecLoopMode` | S32 | 0 | 0 hold-end, 1 loop, 2 ping-pong |
| `FlycamRecSmooth` | bool | true | Catmull-Rom/squad vs linear |
| `FlycamRecAnchorMode` | S32 | 0 | 0 world, 1 self, 2 selection, 3 camera-at-play |
| `FlycamRecFollowAnchor` | bool | false | re-resolve the anchor every frame (ride a moving subject) |
| `FlycamRecUseOperator` | bool | false | layer the handheld operator on playback |

The UI (`panel_flycam_recorder.xml` + `ALPanelFlycamRecorder`) is a shared panel: Record /
Play / Stop / Clear, a scrub slider, time + status readouts, the settings-backed playback
& recording rows, and Save/Load pickers. The transport buttons and scrub are wired in C++
to the singleton; every parameter row is `control_name`-backed so the standalone floater
and the Director Console Takes tab stay in lockstep with no forked state.

---

## 6. Known limitations / non-goals (current)

- **No in-viewer keyframe editing.** You record, scrub, save, and hand-edit the XML.
  There is no add/delete/move-key, no per-key pose/FOV edit, no retiming, no timeline.
- **No in-world path visualization.** The path is invisible; only the live camera shows it.
- **Uniform interpolation.** Catmull-Rom/squad are *uniform* (parameter = key index),
  so non-uniform key spacing (sparse hand edits) yields non-constant speed and can
  overshoot at sharp keys. No arc-length reparameterization, no per-key easing/tangents.
- **Capture density = frame rate.** Recording samples the *final* camera at up to the
  configured Hz but bounded by the live frame rate; a hitch drops samples. Playback is
  time-correct but *rendered* density is live-fps-bound (no deterministic per-output-frame
  evaluation for clean video).
- **Yaw-only anchoring.** Relative playback rotates only about world Z (keeps the horizon
  level); a subject that pitches/rolls isn't followed in those axes.
- **Single take.** One take in memory; no named-take library, no sequencing/cut list.
- **FOV only.** Vertical FOV is keyframed; focus distance / depth-of-field / aperture are
  not (no rack-focus).
- **Global smooth toggle.** Smoothing is all-or-nothing, not per-key.

---

## 7. Research questions (the deliverable)

Please produce a **prioritized roadmap** answering these, with concrete implementation
approaches keyed to the code above and the constraints in §8.

**A. Editing UX (highest interest).**
1. Design an in-viewer **timeline / dope-sheet** for keyframes: add/insert/delete, drag to
   retime, select, multi-select, snap, and edit a single key's camera pose (position,
   orientation, FOV). What is the minimal data-model change to `Keyframe` / the LLSD take
   to support this well (stable key IDs? per-key tangent/ease? labels)?
2. Design an **in-world camera-path editor** reusing the `ALToolPathEdit` precedent: draw
   the spline + key markers in 3D, click/drag a key to reposition, "set this key's
   orientation/FOV from the current camera", insert a key at the playhead. What are the
   picking and gizmo choices?
3. How should "record" and "hand-place keys" coexist (append vs overwrite, insert at
   playhead, re-record a range/overwrite between two times)?

**B. Motion quality.**
4. Should playback offer **constant-speed / arc-length reparameterized** motion and
   **ease-in/ease-out along the whole move** (independent of key spacing)? Give the
   algorithm (build an arc-length LUT over the Catmull-Rom spline; remap playhead→u).
5. Per-key **easing/tangents** (Bezier/TCB/auto/linear/stepped) vs the current global
   smooth toggle — data model + evaluation. How to keep it hand-editable in LLSD?
6. Centripetal vs uniform Catmull-Rom to kill overshoot at sharp hand-edited keys — worth
   it? Edge cases with the squad neighbor selection?
7. **Focus/DoF keyframing** for rack-focus (the viewer has depth-of-field); how to record
   and key focus distance + aperture alongside FOV.

**C. Integration with the viewer's time + camera systems.**
8. **World-clock playback:** should a take optionally advance on the new `LLPresentationTime`
   presentation clock instead of wall time, so a camera move slows/freezes *with* the
   world in slow-motion? What are the failure modes (the recorder currently clamps its own
   dt; §4 `updateCamera`), and the cleanest opt-in?
9. **Fixed-frame / deterministic capture:** for rendering clean video, evaluate the take at
   exact output-frame times (t = frame / output_fps) decoupled from live fps, in lockstep
   with a fixed-frame world capture. Sketch the control flow given the idle-loop dispatch
   in §5.1.
10. **Motion blur:** the absolute-time addressability of `evalPose` enables shutter
    sub-sampling (evaluate N sub-times per output frame). Feasible/worth it here?
11. **Director scenes / multi-camera:** extend from one take to a **named-take library**
    and a **shot/cut timeline** (sequence takes, per-shot camera + Subject + world-speed),
    reusing the Director "scene" concept. Data model?

**D. File format & interoperability (machinima power-user need).**
12. Import/export the take to/from standard camera-animation formats so users round-trip
    with pro tools: Nuke/Houdini **`.chan`**, **glTF** camera + animation, **FBX** camera,
    Blender (script), After Effects camera keyframes. Which are highest-value and how to
    map (global double coords, SL's Z-up + FOV-vertical-radians conventions, frame rate)?
13. Take-format evolution: versioning strategy (currently v2), forward/back compat,
    per-key IDs/labels, embedded lens metadata, and keeping it human-editable.

**E. Robustness / correctness.**
14. Audit the recording path for frame-rate dependence and propose a capture model that is
    density-stable across hitches without changing the "record whatever drives the camera"
    principle.
15. Audit anchoring: yaw-only vs full-orientation follow; anchor loss mid-shot; selection
    changing mid-playback; region crossings (global coords help, but call out pitfalls).
16. Numerical: squad log/exp near-parallel handling, quaternion hemisphere canonicalization
    across load, FOV clamping, very long takes (memory / precision of `F32 mTime`).

---

## 8. Constraints any proposal must respect

- **Engine:** C++17-ish, single-process, single-threaded main-loop camera dispatch (§5.1).
  The camera is one global `LLViewerCamera`; playback must remain a clean branch that
  fully writes it. No server/simulator changes (this is client-side presentation only).
- **Math/types:** `LLVector3d` (global, double), `LLVector3` (agent, float), `LLQuaternion`
  (note its component ctor normalizes — see the `FRQuat` workaround), `LLMatrix3`. SL is
  Z-up; FOV is vertical in radians; positions are global with a 256 m region grid.
- **UI:** XUI (XML) panels + C++ (`LLPanel`, `getChild<T>`, `control_name` settings
  binding, `setCommitCallback`). Shared-panel idiom (one panel embedded by floater + the
  Director Console) — see `ALPanelFlycamRecorder`. In-world tools subclass `LLTool`
  (precedent: `ALToolPathEdit`).
- **Persistence:** takes are LLSD-XML files chosen via `LLFilePickerReplyThread`; parameters
  are `gSavedSettings` (`app_settings/settings.xml`). Keep takes hand-editable.
- **Ship-small:** prefer phases that each deliver a working slice; call out risky landings
  (esp. anything touching the delicate camera dispatch or the interpolation math).

*(All code above is the current implementation, verbatim from
`indra/newview/llflycamrecorder.{h,cpp}`, `alpanelflycamrecorder.{h,cpp}`,
`skins/default/xui/en/panel_flycam_recorder.xml`, and the `llappviewer.cpp` idle dispatch.)*

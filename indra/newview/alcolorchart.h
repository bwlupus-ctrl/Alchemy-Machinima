/** Client-only, session-only colour reference. Never serialised or sent to a sim. */
#ifndef AL_COLOR_CHART_H
#define AL_COLOR_CHART_H
#include "llpointer.h"
#include "llquaternion.h"
#include "llrendertarget.h"
#include "v3dmath.h"
#include "v3math.h"
#include <boost/signals2/connection.hpp>
#include <glm/mat4x4.hpp>
class LLVOVolume;
class LLViewerTexture;
class LLView;

class ALColorChart
{
public:
    static ALColorChart& instance();
    static void toggle();
    static void bind(LLView* root);
    static void shutdownSession(); // never constructs an unused singleton
    void pinHere();
    void idle();
    void renderUnlit(LLRenderTarget* dst, bool exposed,
                     const glm::mat4& projection, const glm::mat4& modelview);
    void renderLegacy(LLRenderTarget* dst, const glm::mat4& projection, const glm::mat4& modelview);
    void presentUnlit(LLRenderTarget* src, bool sensor, const glm::mat4& projection, const glm::mat4& modelview);
    void debugReadback(); // dev-only; called after renderFinalizePresent, before swap
private:
    ALColorChart();
    ~ALColorChart();
    void destroyPrim();
    void releaseResources();
    bool ensureShaders(bool legacy);
    void cameraTransform(LLVector3d& position, LLQuaternion& rotation, LLVector3& scale) const;
    LLVector3 pinnedScale() const;
    void buildTexture();
    void drawChart(LLRenderTarget* dst, const glm::mat4& projection, const glm::mat4& modelview,
                   bool exposed, LLRenderTarget* encoded = nullptr, bool atlas = false, bool screen_sample = false);
    LLPointer<LLVOVolume> mPrim;
    LLPointer<LLViewerTexture> mTexture;
    LLRenderTarget mLinear;
    LLRenderTarget mExposed;
    LLVector3d mPosition;
    LLQuaternion mRotation;
    LLVector3 mScale;
    bool mPinned = false;
    bool mPinFullFrame = false; // layout the pin was made in
    bool mCheckRegionPin = false;
    bool mShaderResourcesRequested = false;
    bool mAtlasFailed = false;
    U64 mShaderGeneration = 0;
    boost::signals2::scoped_connection mShowConnection;
    boost::signals2::scoped_connection mPlacementConnection;
    boost::signals2::scoped_connection mTeleportConnection;
    glm::mat4 mLastMatrix{1.f};
    bool mLastSensor = false;
    U32 mReadbackFrame = 0;
    U32 mLastDrawFrame = ~0u;
};
#endif

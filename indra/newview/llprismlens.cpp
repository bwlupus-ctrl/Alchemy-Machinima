/**
 * @file llprismlens.cpp
 * @brief Capped three-instance, opaque-correct Prism Lens implementation.
 */

#include "llviewerprecompiledheaders.h"

#include "llprismlens.h"

#include "llappviewer.h"
#include "lldrawable.h"
#include "llenvironment.h"
#include "llface.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llnotificationsutil.h"
#include "llplane.h"
#include "llrender.h"
#include "llrendertarget.h"
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llvovolume.h"
#include "pipeline.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

extern bool gCubeSnapshot;

namespace
{
constexpr S32 DEBUG_RECT_X = 96;
constexpr S32 DEBUG_RECT_Y = 96;
constexpr S32 DEBUG_RECT_WIDTH = 512;
constexpr S32 DEBUG_RECT_HEIGHT = 288;
constexpr S32 MIN_PROJECTED_EXTENT = 8;
constexpr S32 MIN_PROJECTED_AREA = 64;
constexpr S32 MIN_VISIBLE_EXTENT = 2;
constexpr S32 MIN_VISIBLE_AREA = 4;
constexpr F32 LENS_PLANE_EPSILON = 0.002f;
constexpr F32 PLANAR_ABSOLUTE_EPSILON = 0.003f;
constexpr F32 PLANAR_RELATIVE_EPSILON = 0.002f;
constexpr F32 PLANAR_NORMAL_COS = 0.9986295f; // cos(3 degrees)
constexpr F32 CLIP_EPSILON = 1e-5f;
constexpr F32 SURFACE_UV_EPSILON = 1e-5f;
constexpr F32 SURFACE_CORNER_EPSILON = 1e-4f;
constexpr F32 MAX_SURFACE_EDGE_COS = 0.052336f; // sin(3 degrees)
constexpr F32 MIN_CULL_HALF_ANGLE = 0.0005f;
constexpr F32 MIN_CULL_VERTICAL_HALF_ANGLE = 0.0436332f; // half of LLCamera's 5 degrees
constexpr F32 MAX_CULL_VERTICAL_HALF_ANGLE = 1.5271631f; // half of LLCamera's 175 degrees
constexpr F32 MAX_CULL_HORIZONTAL_HALF_ANGLE = 1.5699231f;
constexpr U32 MIN_TARGET_EXTENT = 64;
constexpr U32 MAX_TARGET_EXTENT = 1024;

struct PrismRect
{
    S32 mX = 0;
    S32 mY = 0;
    U32 mWidth = 0;
    U32 mHeight = 0;
};

struct PrismFrame
{
    U32 mFrame = 0;
    LLFace* mResolvedFace = nullptr; // Call-scoped; RAII-cleared before returning.
    PrismRect mLensRect;
    PrismRect mDebugRect;
    S32 mMainViewport[4] = { 0, 0, 0, 0 };
    LLPlane mFragmentClipPlane;
    LLVector3 mSurfaceOrigin;
    LLVector3 mSurfaceUDual;
    LLVector3 mSurfaceVDual;
    LLVector3 mWorldSurfaceOrigin;
    LLVector3 mWorldSurfaceUEdge;
    LLVector3 mWorldSurfaceVEdge;
    F32 mCompositeUvScale[2] = { 1.f, 1.f };
    F32 mCompositeUvOffset[2] = { 0.f, 0.f };
    U32 mTargetWidth = 0;
    U32 mTargetHeight = 0;
    F32 mZoom = 2.f;
    F32 mResolutionScale = 1.f;
    F32 mEdgeFeather = 0.f;
    bool mPrepared = false;
    bool mProduced = false;
};

bool prismEnabled()
{
    static LLCachedControl<bool> enabled(gSavedSettings, "PrismLensEnabled", false);
    return enabled;
}

bool prismDebugEnabled()
{
    static LLCachedControl<bool> debug(gSavedSettings, "PrismLensDebug", false);
    return debug;
}

F32 prismZoom()
{
    static LLCachedControl<F32> zoom(gSavedSettings, "PrismLensZoom", 2.f);
    return llclamp(zoom(), 1.f, 8.f);
}

F32 prismResolutionScale()
{
    static LLCachedControl<F32> scale(gSavedSettings, "PrismLensResolutionScale", 1.f);
    return llclamp(scale(), 0.25f, 2.f);
}

F32 prismEdgeFeather()
{
    static LLCachedControl<F32> feather(gSavedSettings, "PrismLensEdgeFeather", 0.f);
    return llmax(feather(), 0.f);
}

bool makeDebugRect(const S32 viewport[4], PrismRect& rect)
{
    if (viewport[2] < 64 || viewport[3] < 64)
    {
        return false;
    }

    rect.mWidth = static_cast<U32>(llmin(viewport[2], DEBUG_RECT_WIDTH));
    rect.mHeight = static_cast<U32>(llmin(viewport[3], DEBUG_RECT_HEIGHT));
    rect.mX = viewport[0] + llmin(DEBUG_RECT_X, viewport[2] - static_cast<S32>(rect.mWidth));
    rect.mY = viewport[1] + llmin(DEBUG_RECT_Y, viewport[3] - static_cast<S32>(rect.mHeight));
    return true;
}

U32 bucketedTargetExtent(F32 projected_extent, F32 scale)
{
    const U32 requested = llclamp(
        static_cast<U32>(llmax(1, ll_round(projected_extent * scale))),
        MIN_TARGET_EXTENT, MAX_TARGET_EXTENT);
    U32 bucket = MIN_TARGET_EXTENT;
    while (bucket < requested && bucket < MAX_TARGET_EXTENT)
    {
        bucket <<= 1;
    }
    return llmin(bucket, MAX_TARGET_EXTENT);
}

F32 clipDistance(const glm::vec4& p, S32 plane)
{
    switch (plane)
    {
        case 0: return p.x + p.w;
        case 1: return p.w - p.x;
        case 2: return p.y + p.w;
        case 3: return p.w - p.y;
        case 4: return p.z + p.w;
        default: return p.w - p.z;
    }
}

using ClipPolygon = std::vector<glm::vec4>;

void clipPolygonAgainstPlane(ClipPolygon& polygon, ClipPolygon& scratch, S32 plane)
{
    if (polygon.empty())
    {
        return;
    }

    scratch.clear();
    scratch.reserve(polygon.size() + 1);
    glm::vec4 previous = polygon.back();
    F32 previous_distance = clipDistance(previous, plane);
    bool previous_inside = previous_distance >= 0.f;

    for (const glm::vec4& current : polygon)
    {
        const F32 current_distance = clipDistance(current, plane);
        const bool current_inside = current_distance >= 0.f;
        if (current_inside != previous_inside)
        {
            const F32 denominator = previous_distance - current_distance;
            if (fabsf(denominator) > CLIP_EPSILON)
            {
                scratch.push_back(previous + (current - previous) *
                    (previous_distance / denominator));
            }
        }
        if (current_inside)
        {
            scratch.push_back(current);
        }
        previous = current;
        previous_distance = current_distance;
        previous_inside = current_inside;
    }
    polygon.swap(scratch);
}

bool deriveRectangularSurface(const LLVolumeFace& volume_face,
                              const std::vector<LLVector3>& surface_positions,
                              const std::vector<LLVector3>& world_positions,
                              F32 fit_tolerance,
                              PrismFrame& frame,
                              std::string& reject_reason)
{
    // Use the volume face's raw geometric UVs only to derive an affine position
    // basis. The shared VB texcoord0 stream is deliberately not consumed because
    // it may contain TE repeat/offset/rotation or animated texture transforms.
    if (!volume_face.mTexCoords ||
        surface_positions.size() != static_cast<size_t>(volume_face.mNumVertices) ||
        world_positions.size() != surface_positions.size())
    {
        reject_reason = "designated face has no geometric UV parameterization";
        return false;
    }

    F32 uv_min_x = std::numeric_limits<F32>::max();
    F32 uv_min_y = std::numeric_limits<F32>::max();
    F32 uv_max_x = -std::numeric_limits<F32>::max();
    F32 uv_max_y = -std::numeric_limits<F32>::max();
    for (S32 i = 0; i < volume_face.mNumVertices; ++i)
    {
        const LLVector2& uv = volume_face.mTexCoords[i];
        if (!std::isfinite(uv.mV[VX]) || !std::isfinite(uv.mV[VY]))
        {
            reject_reason = "designated face has invalid geometric UVs";
            return false;
        }
        uv_min_x = llmin(uv_min_x, uv.mV[VX]);
        uv_min_y = llmin(uv_min_y, uv.mV[VY]);
        uv_max_x = llmax(uv_max_x, uv.mV[VX]);
        uv_max_y = llmax(uv_max_y, uv.mV[VY]);
    }

    const F32 uv_width = uv_max_x - uv_min_x;
    const F32 uv_height = uv_max_y - uv_min_y;
    if (uv_width <= SURFACE_UV_EPSILON || uv_height <= SURFACE_UV_EPSILON)
    {
        reject_reason = "designated face has degenerate geometric UVs";
        return false;
    }

    S32 basis_b = -1;
    S32 basis_c = -1;
    const LLVector2 uv_a = volume_face.mTexCoords[0];
    for (S32 i = 1; i < volume_face.mNumVertices && basis_c < 0; ++i)
    {
        const LLVector2 delta_b = volume_face.mTexCoords[i] - uv_a;
        if (delta_b.magVecSquared() <= SURFACE_UV_EPSILON * SURFACE_UV_EPSILON)
        {
            continue;
        }
        for (S32 j = i + 1; j < volume_face.mNumVertices; ++j)
        {
            const LLVector2 delta_c = volume_face.mTexCoords[j] - uv_a;
            const F32 determinant = delta_b.mV[VX] * delta_c.mV[VY] -
                                    delta_b.mV[VY] * delta_c.mV[VX];
            if (fabsf(determinant) > SURFACE_UV_EPSILON)
            {
                basis_b = i;
                basis_c = j;
                break;
            }
        }
    }
    if (basis_b < 0 || basis_c < 0)
    {
        reject_reason = "designated face UVs do not span a surface";
        return false;
    }

    const LLVector2 delta_b = volume_face.mTexCoords[basis_b] - uv_a;
    const LLVector2 delta_c = volume_face.mTexCoords[basis_c] - uv_a;
    const F32 determinant = delta_b.mV[VX] * delta_c.mV[VY] -
                            delta_b.mV[VY] * delta_c.mV[VX];
    const LLVector3 position_delta_b = surface_positions[basis_b] - surface_positions[0];
    const LLVector3 position_delta_c = surface_positions[basis_c] - surface_positions[0];
    const LLVector3 position_per_u =
        (position_delta_b * delta_c.mV[VY] - position_delta_c * delta_b.mV[VY]) /
        determinant;
    const LLVector3 position_per_v =
        (position_delta_c * delta_b.mV[VX] - position_delta_b * delta_c.mV[VX]) /
        determinant;
    const LLVector3 raw_uv_origin = surface_positions[0] -
        position_per_u * uv_a.mV[VX] - position_per_v * uv_a.mV[VY];
    const LLVector3 surface_origin = raw_uv_origin +
        position_per_u * uv_min_x + position_per_v * uv_min_y;
    const LLVector3 surface_u_edge = position_per_u * uv_width;
    const LLVector3 surface_v_edge = position_per_v * uv_height;
    const LLVector3 world_delta_b = world_positions[basis_b] - world_positions[0];
    const LLVector3 world_delta_c = world_positions[basis_c] - world_positions[0];
    const LLVector3 world_per_u =
        (world_delta_b * delta_c.mV[VY] - world_delta_c * delta_b.mV[VY]) /
        determinant;
    const LLVector3 world_per_v =
        (world_delta_c * delta_b.mV[VX] - world_delta_b * delta_c.mV[VX]) /
        determinant;
    const LLVector3 world_raw_uv_origin = world_positions[0] -
        world_per_u * uv_a.mV[VX] - world_per_v * uv_a.mV[VY];
    const LLVector3 world_surface_origin = world_raw_uv_origin +
        world_per_u * uv_min_x + world_per_v * uv_min_y;
    const LLVector3 world_surface_u_edge = world_per_u * uv_width;
    const LLVector3 world_surface_v_edge = world_per_v * uv_height;

    bool has_corner[4] = { false, false, false, false };
    for (S32 i = 0; i < volume_face.mNumVertices; ++i)
    {
        const LLVector2& uv = volume_face.mTexCoords[i];
        const F32 normalized_u = (uv.mV[VX] - uv_min_x) / uv_width;
        const F32 normalized_v = (uv.mV[VY] - uv_min_y) / uv_height;
        const LLVector3 predicted = surface_origin +
            surface_u_edge * normalized_u + surface_v_edge * normalized_v;
        const LLVector3 predicted_world = world_surface_origin +
            world_surface_u_edge * normalized_u + world_surface_v_edge * normalized_v;
        if ((predicted - surface_positions[i]).magVec() > fit_tolerance ||
            (predicted_world - world_positions[i]).magVec() > fit_tolerance)
        {
            reject_reason = "designated face is not an affine rectangular UV surface";
            return false;
        }

        const bool at_min_u = fabsf(normalized_u) <= SURFACE_CORNER_EPSILON;
        const bool at_max_u = fabsf(normalized_u - 1.f) <= SURFACE_CORNER_EPSILON;
        const bool at_min_v = fabsf(normalized_v) <= SURFACE_CORNER_EPSILON;
        const bool at_max_v = fabsf(normalized_v - 1.f) <= SURFACE_CORNER_EPSILON;
        has_corner[0] |= at_min_u && at_min_v;
        has_corner[1] |= at_max_u && at_min_v;
        has_corner[2] |= at_min_u && at_max_v;
        has_corner[3] |= at_max_u && at_max_v;
    }
    if (!has_corner[0] || !has_corner[1] || !has_corner[2] || !has_corner[3])
    {
        reject_reason = "Prism lens surface-fit requires a four-corner rectangular UV face";
        return false;
    }

    const F32 uu = surface_u_edge * surface_u_edge;
    const F32 uv = surface_u_edge * surface_v_edge;
    const F32 vv = surface_v_edge * surface_v_edge;
    const F32 dual_determinant = uu * vv - uv * uv;
    if (!std::isfinite(uu) || !std::isfinite(uv) || !std::isfinite(vv) ||
        uu <= SURFACE_UV_EPSILON * SURFACE_UV_EPSILON ||
        vv <= SURFACE_UV_EPSILON * SURFACE_UV_EPSILON ||
        dual_determinant <= uu * vv * SURFACE_UV_EPSILON)
    {
        reject_reason = "designated face has a degenerate surface basis";
        return false;
    }

    const F32 world_u_length = world_surface_u_edge.magVec();
    const F32 world_v_length = world_surface_v_edge.magVec();
    if (!std::isfinite(world_u_length) || !std::isfinite(world_v_length) ||
        world_u_length <= F_ALMOST_ZERO || world_v_length <= F_ALMOST_ZERO ||
        fabsf(world_surface_u_edge * world_surface_v_edge) >
            world_u_length * world_v_length * MAX_SURFACE_EDGE_COS)
    {
        reject_reason = "Prism lens surface-fit requires an approximately rectangular face";
        return false;
    }

    frame.mSurfaceOrigin = surface_origin;
    frame.mSurfaceUDual = (surface_u_edge * vv - surface_v_edge * uv) / dual_determinant;
    frame.mSurfaceVDual = (surface_v_edge * uu - surface_u_edge * uv) / dual_determinant;
    frame.mWorldSurfaceOrigin = world_surface_origin;
    frame.mWorldSurfaceUEdge = world_surface_u_edge;
    frame.mWorldSurfaceVEdge = world_surface_v_edge;
    return true;
}

bool selectedFaceIdentity(LLUUID& object_id, S32& te,
                          std::string* reject_reason = nullptr,
                          LLVOVolume** selected_volume = nullptr)
{
    if (selected_volume)
    {
        *selected_volume = nullptr;
    }
    const auto reject = [reject_reason](const char* reason)
    {
        if (reject_reason)
        {
            *reject_reason = reason;
        }
        return false;
    };

    auto selection = LLSelectMgr::getInstance()->getSelection();
    if (selection.isNull())
    {
        return reject("select exactly one valid, non-HUD volume face");
    }

    LLViewerObject* object = nullptr;
    LLSelectNode* node = nullptr;
    U32 selected_object_count = 0;
    U32 selected_face_count = 0;
    // Do not use valid_begin(): mValid only means the asynchronous object-
    // properties reply has arrived. Excluding not-yet-valid nodes can turn a
    // real multi-object selection into an apparently valid single-face one.
    for (auto iter = selection->begin(); iter != selection->end(); ++iter)
    {
        LLSelectNode* candidate_node = *iter;
        LLViewerObject* candidate_object = candidate_node ? candidate_node->getObject() : nullptr;
        if (!candidate_object)
        {
            return reject("selection contains an unavailable object; try again when it finishes loading");
        }
        ++selected_object_count;
        if (selected_object_count > 1)
        {
            return reject("select exactly one face; multi-object or multi-face selections are ambiguous");
        }
        for (S32 candidate_te = 0; candidate_te < candidate_object->getNumTEs(); ++candidate_te)
        {
            if (!candidate_node->isTESelected(candidate_te))
            {
                continue;
            }
            ++selected_face_count;
            object = candidate_object;
            node = candidate_node;
            te = candidate_te;
            if (selected_face_count > 1)
            {
                return reject("select exactly one face; multi-object or multi-face selections are ambiguous");
            }
        }
    }

    if (selected_object_count != 1 || selected_face_count != 1 ||
        !object || !node || object->isDead() ||
        object->isHUDAttachment())
    {
        return reject("select exactly one valid, non-HUD volume face");
    }

    LLVOVolume* volume_object = dynamic_cast<LLVOVolume*>(object);
    LLDrawable* drawable = object->mDrawable.get();
    LLVolume* volume = volume_object ? volume_object->getVolume() : nullptr;
    if (!volume_object || te < 0 || te >= object->getNumTEs() ||
        !drawable || te >= drawable->getNumFaces() ||
        !volume || te >= volume->getNumVolumeFaces())
    {
        return reject("select exactly one valid, non-HUD volume face");
    }

    LLFace* face = drawable->getFace(te);
    if (!face)
    {
        return reject("select exactly one valid, non-HUD volume face");
    }
    if (volume_object->isRiggedMesh() || face->isState(LLFace::RIGGED))
    {
        return reject("Prism lens requires a static (non-rigged) prim face");
    }

    object_id = object->getID();
    if (object_id.isNull())
    {
        return reject("select exactly one valid, non-HUD volume face");
    }
    if (selected_volume)
    {
        *selected_volume = volume_object;
    }
    return true;
}

enum class ESurfaceValidation
{
    VALID,
    TRANSIENT,
    INVALID
};

struct SurfaceGeometry
{
    LLFace* mFace = nullptr;
    LLVector3 mCenter;
    LLVector3 mPlaneNormal;
};

// Resolve and validate the camera-independent part of a lens face. Keeping this
// in one path ensures the Add button cannot claim success for geometry that the
// renderer would immediately reject (or retain forever while the master is off).
ESurfaceValidation validateSurfaceGeometry(
    LLVOVolume* object, S32 te, PrismFrame& frame,
    std::vector<LLVector3>& surface_positions,
    std::vector<LLVector3>& world_positions,
    SurfaceGeometry& geometry, std::string& reject_reason)
{
    geometry = SurfaceGeometry();
    reject_reason.clear();
    if (!object || object->isDead())
    {
        reject_reason = "selected object is no longer available";
        return ESurfaceValidation::TRANSIENT;
    }

    LLDrawable* drawable = object->mDrawable.get();
    LLVolume* volume = object->getVolume();
    if (!drawable || !volume)
    {
        reject_reason = "selected face geometry is still loading; try again";
        return ESurfaceValidation::TRANSIENT;
    }
    if (te < 0 || te >= object->getNumTEs() ||
        te >= drawable->getNumFaces() || te >= volume->getNumVolumeFaces())
    {
        reject_reason = "invalid texture-entry/face index";
        return ESurfaceValidation::INVALID;
    }

    LLFace* face = drawable->getFace(te);
    if (!face || face->getTEOffset() != te || !face->hasGeometry() ||
        !face->getVertexBuffer() || face->getIndicesCount() < 3)
    {
        reject_reason = "selected face geometry is still loading; try again";
        return ESurfaceValidation::TRANSIENT;
    }
    if (object->isRiggedMesh() || face->isState(LLFace::RIGGED))
    {
        reject_reason = "Prism lens requires a static (non-rigged) prim face";
        return ESurfaceValidation::INVALID;
    }

    const LLVolumeFace& volume_face = volume->getVolumeFace(te);
    if (!volume_face.mPositions || !volume_face.mIndices ||
        volume_face.mNumVertices < 3 || volume_face.mNumIndices < 3)
    {
        reject_reason = "designated volume face has no triangle geometry";
        return ESurfaceValidation::INVALID;
    }

    LLMatrix4 surface_matrix = object->getRelativeXform();
    if (drawable->isState(LLDrawable::ANIMATED_CHILD))
    {
        // Animated-child VBs are rebuilt with force_identity=true: scale is
        // baked into position, while the drawable world matrix is applied at draw.
        surface_matrix.initScale(object->getScale());
    }
    const LLMatrix4* model_matrix = nullptr;
    if (drawable->isState(LLDrawable::ANIMATED_CHILD))
    {
        model_matrix = &drawable->getWorldMatrix();
    }
    else if (drawable->isActive())
    {
        model_matrix = &drawable->getRenderMatrix();
    }
    else if (drawable->getRegion())
    {
        model_matrix = &drawable->getRegion()->mRenderMatrix;
    }
    if (!model_matrix)
    {
        reject_reason = "selected face transform is still loading; try again";
        return ESurfaceValidation::TRANSIENT;
    }

    surface_positions.clear();
    surface_positions.reserve(volume_face.mNumVertices);
    world_positions.clear();
    world_positions.reserve(volume_face.mNumVertices);
    LLVector3 center;
    center.setZero();
    LLVector3 min_corner(std::numeric_limits<F32>::max(),
                         std::numeric_limits<F32>::max(),
                         std::numeric_limits<F32>::max());
    LLVector3 max_corner(-std::numeric_limits<F32>::max(),
                         -std::numeric_limits<F32>::max(),
                         -std::numeric_limits<F32>::max());
    for (S32 i = 0; i < volume_face.mNumVertices; ++i)
    {
        const LLVector3 surface =
            LLVector3(volume_face.mPositions[i].getF32ptr()) * surface_matrix;
        const LLVector3 world = surface * (*model_matrix);
        if (!surface.isFinite() || !world.isFinite())
        {
            reject_reason = "designated face contains non-finite geometry";
            return ESurfaceValidation::INVALID;
        }
        surface_positions.push_back(surface);
        world_positions.push_back(world);
        center += world;
        min_corner.setVec(llmin(min_corner.mV[VX], world.mV[VX]),
                          llmin(min_corner.mV[VY], world.mV[VY]),
                          llmin(min_corner.mV[VZ], world.mV[VZ]));
        max_corner.setVec(llmax(max_corner.mV[VX], world.mV[VX]),
                          llmax(max_corner.mV[VY], world.mV[VY]),
                          llmax(max_corner.mV[VZ], world.mV[VZ]));
    }
    center *= 1.f / static_cast<F32>(world_positions.size());

    LLVector3 plane_normal;
    bool found_plane = false;
    for (S32 i = 0; i + 2 < volume_face.mNumIndices; i += 3)
    {
        const U16 ia = volume_face.mIndices[i];
        const U16 ib = volume_face.mIndices[i + 1];
        const U16 ic = volume_face.mIndices[i + 2];
        if (ia >= world_positions.size() || ib >= world_positions.size() ||
            ic >= world_positions.size())
        {
            reject_reason = "designated volume face contains an invalid triangle index";
            return ESurfaceValidation::INVALID;
        }
        LLVector3 normal = (world_positions[ib] - world_positions[ia]) %
                           (world_positions[ic] - world_positions[ia]);
        if (normal.normVec() > F_ALMOST_ZERO)
        {
            plane_normal = normal;
            found_plane = true;
            break;
        }
    }
    if (!found_plane)
    {
        reject_reason = "designated face is degenerate";
        return ESurfaceValidation::INVALID;
    }

    const F32 diagonal = (max_corner - min_corner).magVec();
    const F32 planar_tolerance = llmax(PLANAR_ABSOLUTE_EPSILON,
                                       diagonal * PLANAR_RELATIVE_EPSILON);
    for (const LLVector3& position : world_positions)
    {
        if (fabsf((position - center) * plane_normal) > planar_tolerance)
        {
            reject_reason = "designated face is materially non-planar";
            return ESurfaceValidation::INVALID;
        }
    }
    for (S32 i = 0; i + 2 < volume_face.mNumIndices; i += 3)
    {
        const U16 ia = volume_face.mIndices[i];
        const U16 ib = volume_face.mIndices[i + 1];
        const U16 ic = volume_face.mIndices[i + 2];
        if (ia >= world_positions.size() || ib >= world_positions.size() ||
            ic >= world_positions.size())
        {
            reject_reason = "designated volume face contains an invalid triangle index";
            return ESurfaceValidation::INVALID;
        }
        LLVector3 normal = (world_positions[ib] - world_positions[ia]) %
                           (world_positions[ic] - world_positions[ia]);
        if (normal.normVec() > F_ALMOST_ZERO &&
            fabsf(normal * plane_normal) < PLANAR_NORMAL_COS)
        {
            reject_reason = "designated face triangle normals are non-planar";
            return ESurfaceValidation::INVALID;
        }
    }

    if (!deriveRectangularSurface(volume_face, surface_positions, world_positions,
                                  planar_tolerance, frame, reject_reason))
    {
        return ESurfaceValidation::INVALID;
    }

    geometry.mFace = face;
    geometry.mCenter = center;
    geometry.mPlaneNormal = plane_normal;
    return ESurfaceValidation::VALID;
}

struct PrismInstance
{
    bool mOccupied = false;
    LLUUID mObjectId;
    S32 mTE = -1;
    PrismFrame mFrame;
    bool mHasOutput = false;
    U32 mOutputWidth = 0;
    U32 mOutputHeight = 0;
    U32 mLastRenderedFrame = 0;
    U32 mRetryAfterFrame = 0;
    F32 mOutputUvScale[2] = { 1.f, 1.f };
    F32 mOutputUvOffset[2] = { 0.f, 0.f };
    std::vector<LLVector3> mSurfacePositions;
    std::vector<LLVector3> mWorldPositions;
};

class PrismLensRegistry
{
public:
    static PrismLensRegistry& instance()
    {
        static PrismLensRegistry registry;
        return registry;
    }

    bool canDesignate() const
    {
        return selectedStatus() == LLPrismLens::EDesignationResult::ELIGIBLE;
    }

    LLPrismLens::EDesignationResult selectedStatus(std::string* reason = nullptr) const
    {
        LLUUID object_id;
        S32 te = -1;
        if (!selectedFaceIdentity(object_id, te, reason))
        {
            return LLPrismLens::EDesignationResult::INVALID_SELECTION;
        }
        if (findSlot(object_id, te) >= 0)
        {
            if (reason)
            {
                *reason = "That face is already a Prism lens.";
            }
            return LLPrismLens::EDesignationResult::ALREADY_EXISTS;
        }
        if (count() >= LLPrismLens::MAX_LENSES)
        {
            if (reason)
            {
                *reason = "Maximum of 3 Prism lenses reached.";
            }
            return LLPrismLens::EDesignationResult::AT_CAPACITY;
        }
        if (reason)
        {
            reason->clear();
        }
        return LLPrismLens::EDesignationResult::ELIGIBLE;
    }

    LLPrismLens::EDesignationResult designate(std::string* reason)
    {
        LLUUID object_id;
        S32 te = -1;
        LLVOVolume* selected_volume = nullptr;
        if (!selectedFaceIdentity(object_id, te, reason, &selected_volume))
        {
            return LLPrismLens::EDesignationResult::INVALID_SELECTION;
        }

        if (findSlot(object_id, te) >= 0)
        {
            if (reason)
            {
                *reason = "That face is already a Prism lens.";
            }
            return LLPrismLens::EDesignationResult::ALREADY_EXISTS;
        }
        if (count() >= LLPrismLens::MAX_LENSES)
        {
            if (reason)
            {
                *reason = "Prism lens limit reached. Remove one before adding another.";
            }
            return LLPrismLens::EDesignationResult::AT_CAPACITY;
        }

        // Validate the full camera-independent surface contract before taking a
        // slot or announcing success. This also works while the master effect is
        // disabled, when render-time validation would otherwise never run.
        PrismFrame validation_frame;
        std::vector<LLVector3> surface_positions;
        std::vector<LLVector3> world_positions;
        SurfaceGeometry geometry;
        std::string validation_reason;
        const ESurfaceValidation validation = validateSurfaceGeometry(
            selected_volume, te, validation_frame, surface_positions,
            world_positions, geometry, validation_reason);
        if (validation != ESurfaceValidation::VALID)
        {
            if (reason)
            {
                *reason = validation_reason.empty()
                    ? "selected face is not ready for Prism lens use"
                    : validation_reason;
            }
            return LLPrismLens::EDesignationResult::INVALID_SELECTION;
        }

        for (U32 slot = 0; slot < LLPrismLens::MAX_LENSES; ++slot)
        {
            PrismInstance& lens = mLenses[slot];
            if (lens.mOccupied)
            {
                continue;
            }
            lens = PrismInstance();
            lens.mOccupied = true;
            lens.mObjectId = object_id;
            lens.mTE = te;
            resetFrame(lens, gFrameCount);
            ++mRevision;
            if (reason)
            {
                reason->clear();
            }
            return LLPrismLens::EDesignationResult::ADDED;
        }

        return LLPrismLens::EDesignationResult::AT_CAPACITY;
    }

    bool remove(U32 slot)
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mOccupied)
        {
            return false;
        }
        clearSlot(slot);
        return true;
    }

    void clearAll()
    {
        for (U32 slot = 0; slot < LLPrismLens::MAX_LENSES; ++slot)
        {
            if (mLenses[slot].mOccupied)
            {
                clearSlot(slot);
            }
        }
        gPipeline.releasePrismLensBuffer();
    }

    void releaseRenderResources()
    {
        for (U32 slot = 0; slot < LLPrismLens::MAX_LENSES; ++slot)
        {
            gPipeline.releasePrismLensOutput(slot);
            PrismInstance& lens = mLenses[slot];
            lens.mHasOutput = false;
            lens.mOutputWidth = 0;
            lens.mOutputHeight = 0;
            lens.mRetryAfterFrame = 0;
            lens.mFrame.mProduced = false;
        }
        gPipeline.releasePrismLensBuffer();
        mScratchWidth = 0;
        mScratchHeight = 0;
        mScratchWidthUnderuseFrames = 0;
        mScratchHeightUnderuseFrames = 0;
        mLastRenderedSlot = -1;
    }

    bool hasDesignation() const
    {
        return count() != 0;
    }

    U32 count() const
    {
        U32 result = 0;
        for (const PrismInstance& lens : mLenses)
        {
            result += lens.mOccupied ? 1u : 0u;
        }
        return result;
    }

    U32 revision() const { return mRevision; }

    bool getDesignation(U32 slot, LLPrismLens::Designation& designation) const
    {
        designation = LLPrismLens::Designation();
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mOccupied)
        {
            return false;
        }
        designation.mSlot = slot;
        designation.mObjectId = mLenses[slot].mObjectId;
        designation.mTextureEntry = mLenses[slot].mTE;
        return true;
    }

    void releaseResolvedFace(U32 slot)
    {
        if (slot < LLPrismLens::MAX_LENSES)
        {
            mLenses[slot].mFrame.mResolvedFace = nullptr;
        }
    }

    bool prepare(U32 slot, const S32 viewport[4], const glm::mat4& main_projection,
                  const glm::mat4& main_modelview, const LLViewerCamera& main_camera)
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mOccupied)
        {
            return false;
        }

        PrismInstance& lens = mLenses[slot];
        resetFrame(lens, gFrameCount);
        PrismFrame& mFrame = lens.mFrame;
        const S32 mTE = lens.mTE;
        const auto reject = [this, slot](const std::string& reject_reason)
        {
            return rejectSlot(slot, reject_reason);
        };
        const auto resolveObject = [this, slot](bool reject_invalid)
        {
            return resolveObjectForSlot(slot, reject_invalid);
        };

        mFrame.mZoom = prismZoom();
        mFrame.mResolutionScale = prismResolutionScale();
        mFrame.mEdgeFeather = prismEdgeFeather();
        std::memcpy(mFrame.mMainViewport, viewport, sizeof(mFrame.mMainViewport));
        makeDebugRect(viewport, mFrame.mDebugRect);

        LLVOVolume* object = resolveObject(true);
        if (!object)
        {
            return false;
        }

        std::vector<LLVector3>& surface_positions = lens.mSurfacePositions;
        std::vector<LLVector3>& positions = lens.mWorldPositions;
        SurfaceGeometry geometry;
        std::string surface_reject_reason;
        const ESurfaceValidation validation = validateSurfaceGeometry(
            object, mTE, mFrame, surface_positions, positions,
            geometry, surface_reject_reason);
        if (validation == ESurfaceValidation::TRANSIENT)
        {
            // Drawable, transform, and volume rebuilds are temporary. Retain the
            // local slot and its most recent output until the face resolves again.
            return false;
        }
        if (validation == ESurfaceValidation::INVALID)
        {
            return reject(surface_reject_reason);
        }

        LLFace* face = geometry.mFace;
        const LLVector3 center = geometry.mCenter;
        const LLVector3 plane_normal = geometry.mPlaneNormal;

        const LLVector3 eye = main_camera.getOrigin();
        const LLVector3 surface_normal =
            mFrame.mWorldSurfaceUEdge % mFrame.mWorldSurfaceVEdge;
        if ((eye - center) * surface_normal < 0.f)
        {
            // Keep the generalized view basis right-handed when the back of the
            // geometric UV surface is viewed, then undo that U reversal at sample time.
            mFrame.mCompositeUvScale[0] = -1.f;
            mFrame.mCompositeUvOffset[0] = 1.f;
        }

        LLVector3 keep_normal = plane_normal;
        if ((eye - center) * keep_normal > 0.f)
        {
            keep_normal = -keep_normal;
        }

        const glm::mat4 main_view_projection = main_projection * main_modelview;

        F32 projected_min_x = std::numeric_limits<F32>::max();
        F32 projected_min_y = std::numeric_limits<F32>::max();
        F32 projected_max_x = -std::numeric_limits<F32>::max();
        F32 projected_max_y = -std::numeric_limits<F32>::max();
        bool projected = false;
        F32 min_x = std::numeric_limits<F32>::max();
        F32 min_y = std::numeric_limits<F32>::max();
        F32 max_x = -std::numeric_limits<F32>::max();
        F32 max_y = -std::numeric_limits<F32>::max();
        bool visible = false;
        const auto accumulate_footprint =
            [viewport](const ClipPolygon& polygon, F32& bounds_min_x,
                       F32& bounds_min_y, F32& bounds_max_x,
                       F32& bounds_max_y, bool& has_footprint)
        {
            for (const glm::vec4& clip : polygon)
            {
                if (clip.w <= CLIP_EPSILON)
                {
                    continue;
                }
                const glm::vec2 ndc(clip.x / clip.w, clip.y / clip.w);
                const F32 x = static_cast<F32>(viewport[0]) +
                    (ndc.x * 0.5f + 0.5f) * static_cast<F32>(viewport[2]);
                const F32 y = static_cast<F32>(viewport[1]) +
                    (ndc.y * 0.5f + 0.5f) * static_cast<F32>(viewport[3]);
                if (!std::isfinite(x) || !std::isfinite(y))
                {
                    continue;
                }
                bounds_min_x = llmin(bounds_min_x, x);
                bounds_min_y = llmin(bounds_min_y, y);
                bounds_max_x = llmax(bounds_max_x, x);
                bounds_max_y = llmax(bounds_max_y, y);
                has_footprint = true;
            }
        };

        // Surface-fit guarantees a rectangular affine aperture. Clip that
        // four-corner hull once instead of allocating and clipping every source
        // triangle; holes/extra tessellation can only make this conservative.
        const LLVector3 world_corners[4] =
        {
            mFrame.mWorldSurfaceOrigin,
            mFrame.mWorldSurfaceOrigin + mFrame.mWorldSurfaceUEdge,
            mFrame.mWorldSurfaceOrigin + mFrame.mWorldSurfaceUEdge +
                mFrame.mWorldSurfaceVEdge,
            mFrame.mWorldSurfaceOrigin + mFrame.mWorldSurfaceVEdge
        };
        ClipPolygon polygon;
        ClipPolygon clip_scratch;
        polygon.reserve(8);
        clip_scratch.reserve(8);
        for (const LLVector3& corner : world_corners)
        {
            polygon.push_back(main_view_projection * glm::vec4(
                corner.mV[VX], corner.mV[VY], corner.mV[VZ], 1.f));
        }
        // Retain a surface that crosses the near plane, and measure its full
        // projected size before clipping the footprint to the viewport.
        for (S32 plane = 4; plane < 6 && !polygon.empty(); ++plane)
        {
            clipPolygonAgainstPlane(polygon, clip_scratch, plane);
        }
        accumulate_footprint(polygon, projected_min_x, projected_min_y,
                             projected_max_x, projected_max_y, projected);
        for (S32 plane = 0; plane < 4 && !polygon.empty(); ++plane)
        {
            clipPolygonAgainstPlane(polygon, clip_scratch, plane);
        }
        accumulate_footprint(polygon, min_x, min_y, max_x, max_y, visible);
        if (!projected)
        {
            // Entirely behind the near plane (or beyond the far plane).
            return false;
        }

        const F32 projected_width = projected_max_x - projected_min_x;
        const F32 projected_height = projected_max_y - projected_min_y;
        if (projected_width < static_cast<F32>(MIN_PROJECTED_EXTENT) ||
            projected_height < static_cast<F32>(MIN_PROJECTED_EXTENT) ||
            projected_width * projected_height < static_cast<F32>(MIN_PROJECTED_AREA))
        {
            // Camera distance and clipping can make an otherwise valid lens tiny.
            // Keep the registry entry and retained output; simply skip this frame.
            return false;
        }
        if (!visible)
        {
            return false;
        }

        const S32 viewport_right = viewport[0] + viewport[2];
        const S32 viewport_top = viewport[1] + viewport[3];
        const S32 left = llclamp(static_cast<S32>(floorf(min_x)), viewport[0], viewport_right);
        const S32 bottom = llclamp(static_cast<S32>(floorf(min_y)), viewport[1], viewport_top);
        const S32 right = llclamp(static_cast<S32>(ceilf(max_x)), viewport[0], viewport_right);
        const S32 top = llclamp(static_cast<S32>(ceilf(max_y)), viewport[1], viewport_top);
        const S32 width = right - left;
        const S32 height = top - bottom;
        if (width < MIN_VISIBLE_EXTENT || height < MIN_VISIBLE_EXTENT ||
            width * height < MIN_VISIBLE_AREA)
        {
            // This is a transient visibility condition, not an invalid designation.
            return false;
        }

        mFrame.mLensRect.mX = left;
        mFrame.mLensRect.mY = bottom;
        mFrame.mLensRect.mWidth = static_cast<U32>(width);
        mFrame.mLensRect.mHeight = static_cast<U32>(height);
        // A nearly clipped footprint still needs a finite crop of at least one pixel.
        mFrame.mZoom = llmin(mFrame.mZoom, static_cast<F32>(llmin(width, height)));
        mFrame.mTargetWidth = bucketedTargetExtent(static_cast<F32>(width), mFrame.mResolutionScale);
        mFrame.mTargetHeight = bucketedTargetExtent(static_cast<F32>(height), mFrame.mResolutionScale);
        mFrame.mFragmentClipPlane = LLPlane(center + keep_normal * LENS_PLANE_EPSILON,
                                            keep_normal);
        mFrame.mResolvedFace = face;
        mFrame.mPrepared = true;
        return true;
    }

    LLFace* resolveFaceForComposite(U32 slot)
    {
        if (slot >= LLPrismLens::MAX_LENSES)
        {
            return nullptr;
        }
        PrismInstance& lens = mLenses[slot];
        PrismFrame& frame = lens.mFrame;
        if (!lens.mOccupied || !lens.mHasOutput || !frame.mPrepared ||
            frame.mFrame != gFrameCount)
        {
            return nullptr;
        }
        LLVOVolume* object = resolveObjectForSlot(slot, true);
        if (!object || !object->mDrawable || lens.mTE < 0 ||
            lens.mTE >= object->mDrawable->getNumFaces())
        {
            return nullptr;
        }
        LLFace* face = object->mDrawable->getFace(lens.mTE);
        if (!face || face->getTEOffset() != lens.mTE || !face->hasGeometry() ||
            !face->getVertexBuffer() || face->getIndicesCount() < 3)
        {
            return nullptr;
        }
        if (object->isRiggedMesh() || face->isState(LLFace::RIGGED))
        {
            rejectSlot(slot, "Prism lens requires a static (non-rigged) prim face");
            return nullptr;
        }
        frame.mResolvedFace = face;
        return face;
    }

    S32 chooseRenderSlot()
    {
        // Populate empty outputs first, in stable rotating order.
        for (U32 offset = 0; offset < LLPrismLens::MAX_LENSES; ++offset)
        {
            const U32 slot = (mNextRenderSlot + offset) % LLPrismLens::MAX_LENSES;
            const PrismInstance& lens = mLenses[slot];
            if (lens.mOccupied && lens.mFrame.mPrepared && !lens.mHasOutput &&
                gFrameCount >= lens.mRetryAfterFrame)
            {
                mNextRenderSlot = (slot + 1) % LLPrismLens::MAX_LENSES;
                return static_cast<S32>(slot);
            }
        }

        // Thereafter refresh the most overdue visible lens. Area is only a
        // tie-breaker, so a small lens can never starve.
        S32 best_slot = -1;
        U32 best_age = 0;
        U64 best_area = 0;
        for (U32 offset = 0; offset < LLPrismLens::MAX_LENSES; ++offset)
        {
            const U32 slot = (mNextRenderSlot + offset) % LLPrismLens::MAX_LENSES;
            const PrismInstance& lens = mLenses[slot];
            if (!lens.mOccupied || !lens.mFrame.mPrepared || !lens.mHasOutput ||
                gFrameCount < lens.mRetryAfterFrame)
            {
                continue;
            }
            const U32 age = gFrameCount - lens.mLastRenderedFrame;
            const U64 area = static_cast<U64>(lens.mFrame.mLensRect.mWidth) *
                             static_cast<U64>(lens.mFrame.mLensRect.mHeight);
            if (best_slot < 0 || age > best_age || (age == best_age && area > best_area))
            {
                best_slot = static_cast<S32>(slot);
                best_age = age;
                best_area = area;
            }
        }
        if (best_slot >= 0)
        {
            mNextRenderSlot = (static_cast<U32>(best_slot) + 1) % LLPrismLens::MAX_LENSES;
        }
        return best_slot;
    }

    void markProduced(U32 slot, U32 output_width, U32 output_height)
    {
        if (slot >= LLPrismLens::MAX_LENSES)
        {
            return;
        }
        PrismInstance& lens = mLenses[slot];
        if (lens.mOccupied && lens.mFrame.mFrame == gFrameCount && lens.mFrame.mPrepared)
        {
            lens.mFrame.mProduced = true;
            lens.mHasOutput = true;
            lens.mOutputWidth = output_width;
            lens.mOutputHeight = output_height;
            lens.mLastRenderedFrame = gFrameCount;
            lens.mRetryAfterFrame = 0;
            std::memcpy(lens.mOutputUvScale, lens.mFrame.mCompositeUvScale,
                        sizeof(lens.mOutputUvScale));
            std::memcpy(lens.mOutputUvOffset, lens.mFrame.mCompositeUvOffset,
                        sizeof(lens.mOutputUvOffset));
            mLastRenderedSlot = static_cast<S32>(slot);
        }
    }

    void invalidateOutput(U32 slot, U32 retry_frames = 0)
    {
        if (slot < LLPrismLens::MAX_LENSES)
        {
            PrismInstance& lens = mLenses[slot];
            lens.mHasOutput = false;
            lens.mOutputWidth = 0;
            lens.mOutputHeight = 0;
            lens.mFrame.mProduced = false;
            lens.mRetryAfterFrame = gFrameCount + retry_frames;
        }
    }

    void deferRetry(U32 slot, U32 retry_frames)
    {
        if (slot < LLPrismLens::MAX_LENSES && mLenses[slot].mOccupied)
        {
            mLenses[slot].mRetryAfterFrame = gFrameCount + retry_frames;
        }
    }

    void stabilizeScratchExtent(U32 desired_width, U32 desired_height,
                                U32& width, U32& height)
    {
        constexpr U32 SHRINK_DELAY_FRAMES = 120;
        const auto stabilize_axis = [&](U32 desired, U32& current,
                                       U32& underuse_frames)
        {
            if (current == 0 || desired > current)
            {
                current = desired;
                underuse_frames = 0;
            }
            else if (desired < current)
            {
                if (++underuse_frames >= SHRINK_DELAY_FRAMES)
                {
                    current = desired;
                    underuse_frames = 0;
                }
            }
            else
            {
                underuse_frames = 0;
            }
        };

        stabilize_axis(desired_width, mScratchWidth, mScratchWidthUnderuseFrames);
        stabilize_axis(desired_height, mScratchHeight, mScratchHeightUnderuseFrames);
        width = mScratchWidth;
        height = mScratchHeight;
    }

    void resetScratchExtentAfterFailure()
    {
        // Forget the physical request after scratch or output allocation fails.
        // A smaller lens can then recreate/downsize the shared pack on the next
        // scheduled frame instead of inheriting the failed lens's VRAM pressure.
        mScratchWidth = 0;
        mScratchHeight = 0;
        mScratchWidthUnderuseFrames = 0;
        mScratchHeightUnderuseFrames = 0;
    }

    const PrismFrame* frame(U32 slot) const
    {
        return slot < LLPrismLens::MAX_LENSES && mLenses[slot].mOccupied
            ? &mLenses[slot].mFrame : nullptr;
    }

    const PrismFrame* activeFrame() const
    {
        return mActiveSlot >= 0 ? frame(static_cast<U32>(mActiveSlot)) : nullptr;
    }

    void setActiveSlot(U32 slot)
    {
        mActiveSlot = slot < LLPrismLens::MAX_LENSES ? static_cast<S32>(slot) : -1;
    }

    void clearActiveSlot() { mActiveSlot = -1; }

    bool getOutputRegion(U32 slot, U32& width, U32& height) const
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mHasOutput)
        {
            return false;
        }
        width = mLenses[slot].mOutputWidth;
        height = mLenses[slot].mOutputHeight;
        return width > 0 && height > 0;
    }

    bool getOutputOrientation(U32 slot, F32 scale[2], F32 offset[2]) const
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mHasOutput)
        {
            return false;
        }
        std::memcpy(scale, mLenses[slot].mOutputUvScale,
                    sizeof(mLenses[slot].mOutputUvScale));
        std::memcpy(offset, mLenses[slot].mOutputUvOffset,
                    sizeof(mLenses[slot].mOutputUvOffset));
        return true;
    }

    S32 lastRenderedSlot() const { return mLastRenderedSlot; }

private:
    static void resetFrame(PrismInstance& lens, U32 frame)
    {
        lens.mFrame = PrismFrame();
        lens.mFrame.mFrame = frame;
    }

    S32 findSlot(const LLUUID& object_id, S32 te) const
    {
        for (U32 slot = 0; slot < LLPrismLens::MAX_LENSES; ++slot)
        {
            const PrismInstance& lens = mLenses[slot];
            if (lens.mOccupied && lens.mObjectId == object_id && lens.mTE == te)
            {
                return static_cast<S32>(slot);
            }
        }
        return -1;
    }

    void clearSlot(U32 slot)
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mOccupied)
        {
            return;
        }
        if (mActiveSlot == static_cast<S32>(slot))
        {
            mActiveSlot = -1;
        }
        if (mLastRenderedSlot == static_cast<S32>(slot))
        {
            mLastRenderedSlot = -1;
        }
        gPipeline.releasePrismLensOutput(slot);
        mLenses[slot] = PrismInstance();
        ++mRevision;
        if (!hasDesignation())
        {
            gPipeline.releasePrismLensBuffer();
            mScratchWidth = 0;
            mScratchHeight = 0;
            mScratchWidthUnderuseFrames = 0;
            mScratchHeightUnderuseFrames = 0;
        }
    }

    bool rejectSlot(U32 slot, const std::string& reason)
    {
        if (slot < LLPrismLens::MAX_LENSES && mLenses[slot].mOccupied)
        {
            LLSD args;
            args["MESSAGE"] = llformat(
                "Removed invalid Prism lens %s face %d: %s",
                mLenses[slot].mObjectId.asString().substr(0, 8).c_str(),
                mLenses[slot].mTE, reason.c_str());
            LLNotificationsUtil::add("SystemMessageTip", args);
            LL_WARNS("PrismLens") << "Removing invalid Prism lens "
                                   << mLenses[slot].mObjectId << " TE "
                                   << mLenses[slot].mTE << ": " << reason << LL_ENDL;
            clearSlot(slot);
        }
        return false;
    }

    LLVOVolume* resolveObjectForSlot(U32 slot, bool reject_invalid)
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mOccupied)
        {
            return nullptr;
        }
        const LLUUID object_id = mLenses[slot].mObjectId;
        if (object_id.isNull())
        {
            if (reject_invalid)
            {
                rejectSlot(slot, "object identifier is invalid");
            }
            return nullptr;
        }
        LLViewerObject* object = gObjectList.findObject(object_id);
        if (!object)
        {
            // Objects can leave the local object list while crossing regions,
            // changing draw distance, or streaming back in. Keep the local
            // designation so it resumes when the UUID resolves again.
            return nullptr;
        }
        if (object->isDead())
        {
            if (reject_invalid)
            {
                rejectSlot(slot, "object is dead");
            }
            return nullptr;
        }
        LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
        if (!volume)
        {
            if (reject_invalid)
            {
                rejectSlot(slot, "object is not a volume");
            }
            return nullptr;
        }
        if (object->isHUDAttachment())
        {
            if (reject_invalid)
            {
                rejectSlot(slot, "HUD attachments cannot be Prism Lenses");
            }
            return nullptr;
        }
        return volume;
    }

    PrismInstance mLenses[LLPrismLens::MAX_LENSES];
    U32 mRevision = 1;
    U32 mNextRenderSlot = 0;
    S32 mActiveSlot = -1;
    S32 mLastRenderedSlot = -1;
    U32 mScratchWidth = 0;
    U32 mScratchHeight = 0;
    U32 mScratchWidthUnderuseFrames = 0;
    U32 mScratchHeightUnderuseFrames = 0;
};

struct ScopedActivePrismSlot
{
    ScopedActivePrismSlot(PrismLensRegistry& registry, U32 slot)
        : mRegistry(registry)
    {
        mRegistry.setActiveSlot(slot);
    }

    ~ScopedActivePrismSlot()
    {
        mRegistry.clearActiveSlot();
    }

    PrismLensRegistry& mRegistry;
};

void dirtyEnvironmentShaders()
{
    LLEnvironment::instance().updateSettingsUniforms();
    for (auto shader = LLViewerShaderMgr::instance()->beginShaders();
         shader != LLViewerShaderMgr::instance()->endShaders(); ++shader)
    {
        shader->mUniformsDirty = true;
        if (shader->mRiggedVariant)
        {
            shader->mRiggedVariant->mUniformsDirty = true;
        }
        for (LLGLSLShader& variant : shader->mGLTFVariants)
        {
            variant.mUniformsDirty = true;
        }
    }
}

LLRender::eBlendFactor blendFactorFromGL(GLint factor)
{
    switch (factor)
    {
        case GL_ONE:                     return LLRender::BF_ONE;
        case GL_ZERO:                    return LLRender::BF_ZERO;
        case GL_DST_COLOR:               return LLRender::BF_DEST_COLOR;
        case GL_SRC_COLOR:               return LLRender::BF_SOURCE_COLOR;
        case GL_ONE_MINUS_DST_COLOR:     return LLRender::BF_ONE_MINUS_DEST_COLOR;
        case GL_ONE_MINUS_SRC_COLOR:     return LLRender::BF_ONE_MINUS_SOURCE_COLOR;
        case GL_DST_ALPHA:               return LLRender::BF_DEST_ALPHA;
        case GL_SRC_ALPHA:               return LLRender::BF_SOURCE_ALPHA;
        case GL_ONE_MINUS_DST_ALPHA:     return LLRender::BF_ONE_MINUS_DEST_ALPHA;
        case GL_ONE_MINUS_SRC_ALPHA:     return LLRender::BF_ONE_MINUS_SOURCE_ALPHA;
        default:                         return LLRender::BF_UNDEF;
    }
}

struct ScopedPrismRenderState
{
    ScopedPrismRenderState()
        : mSavedCamera(LLViewerCamera::instance()),
          mSavedProjection(get_current_projection()),
          mSavedModelview(get_current_modelview()),
          mSavedLastProjection(get_last_projection()),
          mSavedLastModelview(get_last_modelview()),
          mSavedGLProjection(gGL.getMatrix(LLRender::MM_PROJECTION)),
          mSavedGLModelview(gGL.getMatrix(LLRender::MM_MODELVIEW)),
          mSavedDeltaModelview(gGLDeltaModelView),
          mSavedInverseDeltaModelview(gGLInverseDeltaModelView),
          mSavedMatrixMode(gGL.getMatrixMode()),
          mSavedRT(gPipeline.mRT),
          mSavedCameraID(LLViewerCamera::sCurCameraID),
          mSavedOcclusion(LLPipeline::sUseOcclusion),
          mSavedUnderWater(LLPipeline::sUnderWaterRender),
          mSavedPrismRender(LLPipeline::sPrismLensRender),
          mSavedVisibleLightCount(LLPipeline::sVisibleLightCount),
          mSavedVisibleFaces(gPipeline.mNumVisibleFaces),
          mSavedVisibleNodes(gPipeline.mNumVisibleNodes),
          mSavedGLLastMatrix(gGLLastMatrix),
          mSavedBoundTarget(LLRenderTarget::getCurrentBoundTarget()),
          mSavedShader(LLGLSLShader::sCurBoundShaderPtr),
          mBlendState(GL_BLEND, LLGLState::DISABLED_STATE),
          mDepthState(GL_TRUE, GL_TRUE, GL_LEQUAL)
    {
        std::memcpy(mSavedGlobalViewport, gGLViewport, sizeof(mSavedGlobalViewport));
        glGetIntegerv(GL_VIEWPORT, mSavedGLViewport);
        glGetBooleanv(GL_COLOR_WRITEMASK, mSavedColorMask);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, mSavedClearColor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &mSavedBlend[0]);
        glGetIntegerv(GL_BLEND_DST_RGB, &mSavedBlend[1]);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &mSavedBlend[2]);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &mSavedBlend[3]);
        mSavedScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
        glGetIntegerv(GL_SCISSOR_BOX, mSavedScissorBox);

        gPipeline.pushRenderTypeMask();
        gGL.setSceneBlendType(LLRender::BT_ALPHA);
        gGL.setColorMask(true, true);
    }

    ~ScopedPrismRenderState()
    {
        LLRenderTarget* bound_target = LLRenderTarget::getCurrentBoundTarget();
        if (bound_target != mSavedBoundTarget && bound_target)
        {
            bound_target->flush();
        }

        gPipeline.popRenderTypeMask();
        gPipeline.mRT = mSavedRT;
        LLPipeline::sPrismLensRender = mSavedPrismRender;
        LLPipeline::sUseOcclusion = mSavedOcclusion;
        LLPipeline::sUnderWaterRender = mSavedUnderWater;
        LLPipeline::sVisibleLightCount = mSavedVisibleLightCount;
        gPipeline.mNumVisibleFaces = mSavedVisibleFaces;
        gPipeline.mNumVisibleNodes = mSavedVisibleNodes;
        gGLLastMatrix = mSavedGLLastMatrix;
        LLViewerCamera::sCurCameraID = mSavedCameraID;

        LLViewerCamera::instance() = mSavedCamera;
        set_current_projection(mSavedProjection);
        set_current_modelview(mSavedModelview);
        set_last_projection(mSavedLastProjection);
        set_last_modelview(mSavedLastModelview);
        gGLDeltaModelView = mSavedDeltaModelview;
        gGLInverseDeltaModelView = mSavedInverseDeltaModelview;

        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.loadMatrix(glm::value_ptr(mSavedGLProjection));
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(glm::value_ptr(mSavedGLModelview));
        gGL.matrixMode(mSavedMatrixMode);
        gGL.syncMatrices();

        std::memcpy(gGLViewport, mSavedGlobalViewport, sizeof(mSavedGlobalViewport));
        glViewport(mSavedGLViewport[0], mSavedGLViewport[1],
                   mSavedGLViewport[2], mSavedGLViewport[3]);
        glClearColor(mSavedClearColor[0], mSavedClearColor[1],
                     mSavedClearColor[2], mSavedClearColor[3]);
        glScissor(mSavedScissorBox[0], mSavedScissorBox[1],
                  mSavedScissorBox[2], mSavedScissorBox[3]);
        if (mSavedScissorEnabled)
        {
            glEnable(GL_SCISSOR_TEST);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }

        gGL.setColorMask(mSavedColorMask[0] != GL_FALSE,
                         mSavedColorMask[1] != GL_FALSE,
                         mSavedColorMask[2] != GL_FALSE,
                         mSavedColorMask[3] != GL_FALSE);

        const LLRender::eBlendFactor src_rgb = blendFactorFromGL(mSavedBlend[0]);
        const LLRender::eBlendFactor dst_rgb = blendFactorFromGL(mSavedBlend[1]);
        const LLRender::eBlendFactor src_alpha = blendFactorFromGL(mSavedBlend[2]);
        const LLRender::eBlendFactor dst_alpha = blendFactorFromGL(mSavedBlend[3]);
        if (src_rgb != LLRender::BF_UNDEF && dst_rgb != LLRender::BF_UNDEF &&
            src_alpha != LLRender::BF_UNDEF && dst_alpha != LLRender::BF_UNDEF)
        {
            gGL.blendFunc(src_rgb, dst_rgb, src_alpha, dst_alpha);
        }
        else
        {
            gGL.setSceneBlendType(LLRender::BT_ALPHA);
        }

        if (mClipUniformsChanged)
        {
            // sPrismLensRender, camera, and matrices are already back to their
            // main-view values before rebuilding and dirtying the uniform cache.
            dirtyEnvironmentShaders();
        }

        if (mSavedShader && mSavedShader->isComplete())
        {
            mSavedShader->bind();
        }
        else
        {
            LLGLSLShader::unbind();
        }

        llassert(LLRenderTarget::getCurrentBoundTarget() == mSavedBoundTarget);
    }

    void activateClipUniforms()
    {
        mClipUniformsChanged = true;
        dirtyEnvironmentShaders();
    }

    LLViewerCamera mSavedCamera;
    glm::mat4 mSavedProjection;
    glm::mat4 mSavedModelview;
    glm::mat4 mSavedLastProjection;
    glm::mat4 mSavedLastModelview;
    glm::mat4 mSavedGLProjection;
    glm::mat4 mSavedGLModelview;
    glm::mat4 mSavedDeltaModelview;
    glm::mat4 mSavedInverseDeltaModelview;
    LLRender::eMatrixMode mSavedMatrixMode;
    S32 mSavedGlobalViewport[4];
    GLint mSavedGLViewport[4];
    GLboolean mSavedColorMask[4];
    GLfloat mSavedClearColor[4];
    GLint mSavedBlend[4];
    GLboolean mSavedScissorEnabled = GL_FALSE;
    GLint mSavedScissorBox[4] = { 0, 0, 0, 0 };
    LLPipeline::RenderTargetPack* mSavedRT;
    LLViewerCamera::eCameraID mSavedCameraID;
    S32 mSavedOcclusion;
    bool mSavedUnderWater;
    bool mSavedPrismRender;
    S32 mSavedVisibleLightCount;
    S32 mSavedVisibleFaces;
    S32 mSavedVisibleNodes;
    const LLMatrix4* mSavedGLLastMatrix;
    LLRenderTarget* mSavedBoundTarget;
    LLGLSLShader* mSavedShader;
    bool mClipUniformsChanged = false;
    LLGLState mBlendState;
    LLGLDepthTest mDepthState;
};
} // anonymous namespace

namespace LLPrismLens
{
EDesignationResult selectedFaceStatus(std::string* reason)
{
    return PrismLensRegistry::instance().selectedStatus(reason);
}

bool canDesignateSelectedFace()
{
    return PrismLensRegistry::instance().canDesignate();
}

EDesignationResult designateSelectedFace(std::string* reason)
{
    return PrismLensRegistry::instance().designate(reason);
}

bool removeDesignation(U32 slot)
{
    return PrismLensRegistry::instance().remove(slot);
}

void clearDesignations()
{
    PrismLensRegistry::instance().clearAll();
}

bool hasDesignation()
{
    return PrismLensRegistry::instance().hasDesignation();
}

U32 designationCount()
{
    return PrismLensRegistry::instance().count();
}

U32 designationRevision()
{
    return PrismLensRegistry::instance().revision();
}

bool getDesignation(U32 slot, Designation& designation)
{
    return PrismLensRegistry::instance().getDesignation(slot, designation);
}

void renderAuxiliaryView()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    PrismLensRegistry& registry = PrismLensRegistry::instance();
    static bool was_enabled = false;
    const bool enabled = prismEnabled();
    if (!enabled)
    {
        // Reclaim bounded Prism VRAM exactly once on the transition while
        // preserving local registry identities for a later re-enable.
        if (was_enabled)
        {
            registry.releaseRenderResources();
        }
        was_enabled = false;
        return;
    }
    was_enabled = true;

    // Enabled but empty remains a complete no-op.
    if (!registry.hasDesignation())
    {
        return;
    }
    if (LLPipeline::sPrismLensRender || !LLPipeline::sRenderDeferred || gCubeSnapshot)
    {
        return;
    }

    S32 main_viewport[4];
    std::memcpy(main_viewport, gGLViewport, sizeof(main_viewport));
    const glm::mat4 main_projection = get_current_projection();
    const glm::mat4 main_modelview = get_current_modelview();
    const LLViewerCamera main_camera = LLViewerCamera::instance();

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Prism prepare visible lenses");
        for (U32 slot = 0; slot < MAX_LENSES; ++slot)
        {
            registry.prepare(slot, main_viewport, main_projection,
                             main_modelview, main_camera);
            // Prepared surface data is copied by value. Never retain a drawable-owned
            // LLFace beyond this preparation call.
            registry.releaseResolvedFace(slot);
        }
    }

    const S32 render_slot = registry.chooseRenderSlot();
    if (render_slot < 0)
    {
        return;
    }

    const U32 slot = static_cast<U32>(render_slot);
    const PrismFrame* frame_ptr = registry.frame(slot);
    if (!frame_ptr || frame_ptr->mTargetWidth == 0 || frame_ptr->mTargetHeight == 0)
    {
        return;
    }
    const PrismFrame& frame = *frame_ptr;
    const U32 output_width = frame.mTargetWidth;
    const U32 output_height = frame.mTargetHeight;
    U32 render_width = output_width;
    U32 render_height = output_height;
    {
        // The deferred scratch pack is expensive to recreate. Grow immediately,
        // but require two seconds of continuous under-use before shrinking.
        // Rendering/copying deliberately use this physical extent because an
        // LLRenderTarget bind resets the viewport to the full target.
        registry.stabilizeScratchExtent(render_width, render_height,
                                        render_width, render_height);
    }
    bool produced = false;
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Prism one scheduled auxiliary update");
        ScopedActivePrismSlot active_slot(registry, slot);
        ScopedPrismRenderState scoped_state;
        if (!gPipeline.allocatePrismLensBuffer(render_width, render_height))
        {
            registry.deferRetry(slot, 30);
            registry.resetScratchExtentAfterFailure();
            return;
        }
        LLPipeline::sPrismLensRender = true;
        LLPipeline::sUseOcclusion = 0;
        LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_PRISM_LENS;
        gPipeline.mRT = &gPipeline.mPrismLensRT;

        const glm::vec3 eye(main_camera.getOrigin().mV[VX],
                            main_camera.getOrigin().mV[VY],
                            main_camera.getOrigin().mV[VZ]);
        const glm::vec3 surface_origin(frame.mWorldSurfaceOrigin.mV[VX],
                                       frame.mWorldSurfaceOrigin.mV[VY],
                                       frame.mWorldSurfaceOrigin.mV[VZ]);
        glm::vec3 surface_u(frame.mWorldSurfaceUEdge.mV[VX],
                            frame.mWorldSurfaceUEdge.mV[VY],
                            frame.mWorldSurfaceUEdge.mV[VZ]);
        const glm::vec3 surface_v(frame.mWorldSurfaceVEdge.mV[VX],
                                  frame.mWorldSurfaceVEdge.mV[VY],
                                  frame.mWorldSurfaceVEdge.mV[VZ]);
        if (frame.mCompositeUvScale[0] < 0.f)
        {
            surface_u = -surface_u;
        }

        // Generalized perspective through the central 1/zoom sub-quad. The
        // full auxiliary texture is later mapped over the full geometric face.
        const glm::vec3 surface_center = surface_origin +
            0.5f * (glm::vec3(frame.mWorldSurfaceUEdge.mV[VX],
                              frame.mWorldSurfaceUEdge.mV[VY],
                              frame.mWorldSurfaceUEdge.mV[VZ]) + surface_v);
        const glm::vec3 sub_u = surface_u / frame.mZoom;
        const glm::vec3 sub_v = surface_v / frame.mZoom;
        const glm::vec3 pa = surface_center - 0.5f * (sub_u + sub_v);
        const glm::vec3 pb = pa + sub_u;
        const glm::vec3 pc = pa + sub_v;
        const glm::vec3 vr = glm::normalize(pb - pa);
        const glm::vec3 vu = glm::normalize(pc - pa);
        const glm::vec3 vn = glm::normalize(glm::cross(vr, vu));
        const glm::vec3 va = pa - eye;
        const glm::vec3 vb = pb - eye;
        const glm::vec3 vc = pc - eye;
        const F32 eye_distance = -glm::dot(vn, va);
        const F32 near_clip = main_camera.getNear();
        const F32 far_clip = main_camera.getFar();
        if (!std::isfinite(eye_distance) || eye_distance <= CLIP_EPSILON ||
            near_clip <= 0.f || far_clip <= near_clip)
        {
            LL_WARNS("PrismLens") << "Skipping degenerate generalized projection" << LL_ENDL;
            registry.deferRetry(slot, 5);
            return;
        }

        const F32 left = glm::dot(vr, va) * near_clip / eye_distance;
        const F32 right = glm::dot(vr, vb) * near_clip / eye_distance;
        const F32 bottom = glm::dot(vu, va) * near_clip / eye_distance;
        const F32 top = glm::dot(vu, vc) * near_clip / eye_distance;
        if (!std::isfinite(left) || !std::isfinite(right) ||
            !std::isfinite(bottom) || !std::isfinite(top) ||
            right <= left || top <= bottom)
        {
            LL_WARNS("PrismLens") << "Skipping invalid generalized frustum" << LL_ENDL;
            registry.deferRetry(slot, 5);
            return;
        }

        // Culling stays symmetric, but its half-angles contain every edge of
        // the exact off-axis sub-quad frustum used for rendering.
        F32 h_half = atanf(llmax(fabsf(left), fabsf(right)) / near_clip);
        F32 v_half = atanf(llmax(fabsf(bottom), fabsf(top)) / near_clip);
        if (!std::isfinite(h_half) || !std::isfinite(v_half))
        {
            // Degenerate projection input must fail open for culling.
            h_half = MAX_CULL_HORIZONTAL_HALF_ANGLE;
            v_half = MAX_CULL_VERTICAL_HALF_ANGLE;
        }
        else
        {
            h_half = llclamp(h_half, MIN_CULL_HALF_ANGLE,
                             MAX_CULL_HORIZONTAL_HALF_ANGLE);
            v_half = llclamp(v_half, MIN_CULL_VERTICAL_HALF_ANGLE,
                             MAX_CULL_VERTICAL_HALF_ANGLE);
            // LLCamera clamps aspect to 50. Widen vertically when necessary so
            // that clamp cannot shrink the required horizontal half-angle.
            v_half = llmax(v_half, atanf(tanf(h_half) / MAX_ASPECT_RATIO));
        }
        F32 cull_aspect = tanf(h_half) / tanf(v_half);
        if (!std::isfinite(cull_aspect) || cull_aspect <= 0.f)
        {
            h_half = MAX_CULL_HORIZONTAL_HALF_ANGLE;
            v_half = MAX_CULL_VERTICAL_HALF_ANGLE;
            cull_aspect = MAX_ASPECT_RATIO;
        }

        // Delay any growth that replaces a retained output until all projection
        // validation has succeeded. An early degenerate-view return must leave
        // the previous cached beauty intact.
        if (!gPipeline.allocatePrismLensOutput(slot, output_width, output_height))
        {
            // Avoid starving the other retained lenses if VRAM allocation keeps
            // failing for this slot.
            registry.invalidateOutput(slot, 30);
            registry.resetScratchExtentAfterFailure();
            return;
        }

        LLViewerCamera lens_camera = main_camera;
        lens_camera.setAxes(LLVector3(-vn.x, -vn.y, -vn.z),
                            LLVector3(-vr.x, -vr.y, -vr.z),
                            LLVector3(vu.x, vu.y, vu.z));
        lens_camera.setAspect(cull_aspect);
        lens_camera.setViewHeightInPixels(static_cast<S32>(render_height));
        // NETWORK SAFETY: temporary lens FOV mutation is value-only and local.
        lens_camera.setViewNoBroadcast(2.f * v_half);

        LLVector3 keep_normal;
        frame.mFragmentClipPlane.getVector3(keep_normal);
        const LLVector3 keep_point = keep_normal * -frame.mFragmentClipPlane[3];
        // LLCamera's AABB convention rejects positive plane distance, opposite the
        // fragment predicate. Invert only the coarse-cull plane so both keep the
        // half-space pointing away from the eye.
        LLPlane cull_plane(keep_point, -keep_normal);
        lens_camera.setUserClipPlane(cull_plane);
        LLViewerCamera::instance() = lens_camera;

        const glm::mat4 prism_projection =
            glm::frustum(left, right, bottom, top, near_clip, far_clip);
        const glm::mat4 prism_modelview = glm::lookAt(eye, eye - vn, vu);
        set_current_projection(prism_projection);
        set_current_modelview(prism_modelview);
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.loadMatrix(glm::value_ptr(prism_projection));
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(glm::value_ptr(prism_modelview));

        gGLViewport[0] = 0;
        gGLViewport[1] = 0;
        gGLViewport[2] = static_cast<S32>(render_width);
        gGLViewport[3] = static_cast<S32>(render_height);
        glViewport(0, 0, static_cast<GLsizei>(render_width),
                   static_cast<GLsizei>(render_height));
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, static_cast<GLsizei>(render_width),
                  static_cast<GLsizei>(render_height));
        LLViewerCamera::updateFrustumPlanes(LLViewerCamera::instance(), false, false, true);

        // Rebuild environment uniforms after the prism flag, camera, matrices, and
        // fragment clip plane are installed. The scope restores the main cache.
        scoped_state.activateClipUniforms();

        static LLCullResult prism_cull;
        prism_cull.clear();
        gPipeline.updateCull(LLViewerCamera::instance(), prism_cull);
        gPipeline.stateSort(LLViewerCamera::instance(), prism_cull);

        LLPipeline::RenderTargetPack& rt = gPipeline.mPrismLensRT;
        rt.deferredScreen.bindTarget();
        glClearColor(0.f, 0.f, 0.f, 0.f);
        rt.deferredScreen.clear();
        gPipeline.renderGeomDeferred(LLViewerCamera::instance(), false);
        rt.deferredScreen.flush();
        gPipeline.renderDeferredLighting();

        // renderDeferredLighting() flushes the scratch HDR screen. Copy the
        // finished beauty while all copy-related GL mutations remain covered by
        // the complete state scope; publish only after that scope closes.
        gPipeline.mPrismLensOutput[slot].copyContents(
            rt.screen,
            0, 0, render_width, render_height,
            0, 0, output_width, output_height,
            GL_COLOR_BUFFER_BIT,
            render_width == output_width && render_height == output_height
                ? GL_NEAREST : GL_LINEAR);
        produced = true;
    }

    // Publish only after the scoped camera/matrix/viewport/target/uniform
    // restoration has completed successfully.
    if (produced)
    {
        registry.markProduced(slot, output_width, output_height);
    }
}

U32 getCompositeStates(LLRenderTarget* screen_target, CompositeState* states,
                       U32 capacity)
{
    if (!states || capacity == 0 || !prismEnabled() || LLPipeline::sPrismLensRender ||
        screen_target != &gPipeline.mMainRT.screen ||
        LLRenderTarget::getCurrentBoundTarget() != screen_target)
    {
        return 0;
    }

    PrismLensRegistry& registry = PrismLensRegistry::instance();
    U32 state_count = 0;
    for (U32 slot = 0; slot < MAX_LENSES && state_count < capacity; ++slot)
    {
        const PrismFrame* frame_ptr = registry.frame(slot);
        LLRenderTarget& output = gPipeline.mPrismLensOutput[slot];
        U32 output_width = 0;
        U32 output_height = 0;
        F32 output_uv_scale[2];
        F32 output_uv_offset[2];
        if (!frame_ptr || !frame_ptr->mPrepared || frame_ptr->mFrame != gFrameCount ||
            frame_ptr->mMainViewport[2] <= 0 || frame_ptr->mMainViewport[3] <= 0 ||
            !output.isComplete() ||
            !registry.getOutputRegion(slot, output_width, output_height) ||
            !registry.getOutputOrientation(slot, output_uv_scale, output_uv_offset) ||
            output_width > output.getWidth() || output_height > output.getHeight())
        {
            continue;
        }

        LLFace* face = registry.resolveFaceForComposite(slot);
        if (!face)
        {
            continue;
        }

        const PrismFrame& frame = *frame_ptr;
        CompositeState& state = states[state_count];
        state = CompositeState();
        state.mSlot = slot;
        const F32 inv_width = 1.f / static_cast<F32>(frame.mMainViewport[2]);
        const F32 inv_height = 1.f / static_cast<F32>(frame.mMainViewport[3]);
        std::memcpy(state.mSurfaceOrigin, frame.mSurfaceOrigin.mV, sizeof(state.mSurfaceOrigin));
        std::memcpy(state.mSurfaceUDual, frame.mSurfaceUDual.mV, sizeof(state.mSurfaceUDual));
        std::memcpy(state.mSurfaceVDual, frame.mSurfaceVDual.mV, sizeof(state.mSurfaceVDual));

        // Retained outputs grow but never shrink. Map the logical copied region
        // onto texel centers so filtering cannot bleed into stale capacity pixels.
        const F32 region_scale_x = static_cast<F32>(output_width - 1u) /
                                   static_cast<F32>(output.getWidth());
        const F32 region_scale_y = static_cast<F32>(output_height - 1u) /
                                   static_cast<F32>(output.getHeight());
        state.mUvScale[0] = output_uv_scale[0] * region_scale_x;
        state.mUvScale[1] = output_uv_scale[1] * region_scale_y;
        state.mUvOffset[0] = 0.5f / static_cast<F32>(output.getWidth()) +
                             output_uv_offset[0] * region_scale_x;
        state.mUvOffset[1] = 0.5f / static_cast<F32>(output.getHeight()) +
                             output_uv_offset[1] * region_scale_y;

        const F32 scale_x = static_cast<F32>(screen_target->getWidth()) * inv_width;
        const F32 scale_y = static_cast<F32>(screen_target->getHeight()) * inv_height;
        const S32 scissor_left = llclamp(ll_round(
            static_cast<F32>(frame.mLensRect.mX - frame.mMainViewport[0]) * scale_x),
            0, static_cast<S32>(screen_target->getWidth()) - 1);
        const S32 scissor_bottom = llclamp(ll_round(
            static_cast<F32>(frame.mLensRect.mY - frame.mMainViewport[1]) * scale_y),
            0, static_cast<S32>(screen_target->getHeight()) - 1);
        const S32 scissor_right = llclamp(ll_round(
            static_cast<F32>(frame.mLensRect.mX - frame.mMainViewport[0] +
                             static_cast<S32>(frame.mLensRect.mWidth)) * scale_x),
            scissor_left + 1, static_cast<S32>(screen_target->getWidth()));
        const S32 scissor_top = llclamp(ll_round(
            static_cast<F32>(frame.mLensRect.mY - frame.mMainViewport[1] +
                             static_cast<S32>(frame.mLensRect.mHeight)) * scale_y),
            scissor_bottom + 1, static_cast<S32>(screen_target->getHeight()));
        state.mScissor[0] = scissor_left;
        state.mScissor[1] = scissor_bottom;
        state.mScissor[2] = scissor_right - scissor_left;
        state.mScissor[3] = scissor_top - scissor_bottom;
        state.mFace = face;
        state.mEdgeFeather = frame.mEdgeFeather;
        registry.releaseResolvedFace(slot);
        ++state_count;
    }
    return state_count;
}

bool getActiveClipPlane(LLPlane& plane)
{
    const PrismFrame* frame = PrismLensRegistry::instance().activeFrame();
    if (!LLPipeline::sPrismLensRender || !frame || !frame->mPrepared ||
        frame->mFrame != gFrameCount)
    {
        return false;
    }
    plane = frame->mFragmentClipPlane;
    return true;
}

void compositeDebug()
{
    PrismLensRegistry& registry = PrismLensRegistry::instance();
    const S32 debug_slot = registry.lastRenderedSlot();
    const PrismFrame* frame = debug_slot >= 0
        ? registry.frame(static_cast<U32>(debug_slot)) : nullptr;
    U32 source_width = 0;
    U32 source_height = 0;
    if (!prismEnabled() || !prismDebugEnabled() || LLPipeline::sPrismLensRender ||
        !frame || frame->mFrame != gFrameCount ||
        !registry.getOutputRegion(static_cast<U32>(debug_slot), source_width, source_height) ||
        !gPipeline.mPrismLensOutput[debug_slot].isComplete() ||
        frame->mDebugRect.mWidth == 0 || frame->mDebugRect.mHeight == 0)
    {
        return;
    }

    LLRenderTarget& source = gPipeline.mPrismLensOutput[debug_slot];
    LLRenderTarget& destination = gPipeline.mMainRT.screen;
    const F32 scale_x = static_cast<F32>(destination.getWidth()) /
                        static_cast<F32>(frame->mMainViewport[2]);
    const F32 scale_y = static_cast<F32>(destination.getHeight()) /
                        static_cast<F32>(frame->mMainViewport[3]);
    const S32 destination_x = ll_round(
        static_cast<F32>(frame->mDebugRect.mX - frame->mMainViewport[0]) * scale_x);
    const S32 destination_y = ll_round(
        static_cast<F32>(frame->mDebugRect.mY - frame->mMainViewport[1]) * scale_y);
    const S32 destination_width = ll_round(
        static_cast<F32>(frame->mDebugRect.mWidth) * scale_x);
    const S32 destination_height = ll_round(
        static_cast<F32>(frame->mDebugRect.mHeight) * scale_y);
    destination.copyContents(
        source,
        0, 0, source_width, source_height,
        destination_x, destination_y,
        destination_x + destination_width,
        destination_y + destination_height,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
}
} // namespace LLPrismLens

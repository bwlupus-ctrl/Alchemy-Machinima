/**
 * @file alavatarstagingmodel.h
 * @brief Viewer-independent math for local avatar blocking.
 */

#ifndef AL_ALAVATARSTAGINGMODEL_H
#define AL_ALAVATARSTAGINGMODEL_H

#include <cstddef>
#include <vector>

namespace ALAvatarStagingModel
{
    constexpr double PI = 3.14159265358979323846;
    constexpr double TAU_SECONDS = 0.25;
    constexpr double MAX_YAW_RATE = PI; // 180 degrees/second
    constexpr double MAX_POSITION_RATE = 4.0;
    constexpr double MAX_OFFSET_METERS = 32.0;
    constexpr double PIN_RELEASE_METERS = 2.0;
    constexpr double PIN_REPIN_METERS = 1.75;
    constexpr double DISCONTINUITY_METERS = 5.0;

    struct Vec3
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;

        Vec3() = default;
        Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

        Vec3 operator+(const Vec3& rhs) const;
        Vec3 operator-(const Vec3& rhs) const;
        Vec3 operator*(double scale) const;
    };

    struct ScalarSpring
    {
        double value = 0.0;
        double velocity = 0.0;
        bool initialized = false;
    };

    struct VectorSpring
    {
        Vec3 value;
        Vec3 velocity;
        bool initialized = false;
    };

    enum class Formation
    {
        CIRCLE_IN,
        CIRCLE_OUT,
        ARC,
        LINE,
        GRID
    };

    struct FormationSlot
    {
        Vec3 foot;
        double yaw = 0.0;
    };

    bool finite(double value);
    bool finite(const Vec3& value);
    double clampFinite(double value, double minimum, double maximum,
                       double fallback = 0.0);
    Vec3 clampOffset(const Vec3& value);
    double wrapYaw(double radians);
    double shortestArc(double from, double to);
    double faceYaw(const Vec3& from, const Vec3& target,
                   double fallback = 0.0);
    double distance(const Vec3& lhs, const Vec3& rhs);
    double distanceXY(const Vec3& lhs, const Vec3& rhs);
    Vec3 clampOffsetXY(const Vec3& value);
    Vec3 clampPinOffset(const Vec3& value);
    Vec3 makePinTarget(const Vec3& requested_position, const Vec3& anchor,
                       double visible_z_offset);
    Vec3 pinTargetOffset(const Vec3& pin, const Vec3& simulated_position);
    Vec3 localOffsetToWorld(const Vec3& value, double world_yaw);
    Vec3 worldOffsetToLocal(const Vec3& value, double local_frame_yaw);
    bool centroid(const std::vector<Vec3>& points, Vec3& result);
    bool reuseFrozenCentroid(bool from_centroid, bool same_dancer_set,
                             const Vec3& centre,
                             const std::vector<Vec3>& dancer_positions,
                             double maximum_distance = 64.0);

    double stepCritical(ScalarSpring& state, double target, double dt,
                        double tau = TAU_SECONDS,
                        double max_rate = MAX_YAW_RATE,
                        bool angular = false);
    Vec3 stepCritical(VectorSpring& state, const Vec3& target, double dt,
                      double tau = TAU_SECONDS,
                      double max_rate = MAX_POSITION_RATE);
    void snap(ScalarSpring& state, double value);
    void snap(VectorSpring& state, const Vec3& value);

    std::vector<double> pairingYaws(const std::vector<Vec3>& ordered_feet,
                                     const Vec3& centre);
    std::vector<FormationSlot> formationSlots(Formation formation,
                                               std::size_t count,
                                               double radius_or_spacing,
                                               double arc_radians = PI);

    bool updatePinState(bool requested, bool currently_pinned,
                        const Vec3& simulated_position,
                        const Vec3& anchor);
    bool isDiscontinuity(const Vec3& previous, const Vec3& current);
}

#endif

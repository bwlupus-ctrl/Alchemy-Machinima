/**
 * @file alavatarstagingmodel.cpp
 * @brief Viewer-independent math for local avatar blocking.
 */

#include "linden_common.h"

#include "alavatarstagingmodel.h"

#include <algorithm>
#include <cmath>

namespace ALAvatarStagingModel
{
    namespace
    {
        double length(const Vec3& value)
        {
            return std::sqrt(value.x * value.x + value.y * value.y +
                             value.z * value.z);
        }

        double safeDt(double dt)
        {
            return clampFinite(dt, 0.0, 0.25, 0.0);
        }
    }

    Vec3 Vec3::operator+(const Vec3& rhs) const
    {
        return Vec3(x + rhs.x, y + rhs.y, z + rhs.z);
    }

    Vec3 Vec3::operator-(const Vec3& rhs) const
    {
        return Vec3(x - rhs.x, y - rhs.y, z - rhs.z);
    }

    Vec3 Vec3::operator*(double scale) const
    {
        return Vec3(x * scale, y * scale, z * scale);
    }

    bool finite(double value)
    {
        return std::isfinite(value);
    }

    bool finite(const Vec3& value)
    {
        return finite(value.x) && finite(value.y) && finite(value.z);
    }

    double clampFinite(double value, double minimum, double maximum,
                       double fallback)
    {
        if (!finite(value))
        {
            value = fallback;
        }
        return std::clamp(value, minimum, maximum);
    }

    Vec3 clampOffset(const Vec3& value)
    {
        if (!finite(value))
        {
            return Vec3();
        }
        const double magnitude = length(value);
        if (magnitude > MAX_OFFSET_METERS)
        {
            return value * (MAX_OFFSET_METERS / magnitude);
        }
        return value;
    }

    double wrapYaw(double radians)
    {
        if (!finite(radians))
        {
            return 0.0;
        }
        radians = std::fmod(radians + PI, 2.0 * PI);
        if (radians < 0.0)
        {
            radians += 2.0 * PI;
        }
        return radians - PI;
    }

    double shortestArc(double from, double to)
    {
        return wrapYaw(to - from);
    }

    double faceYaw(const Vec3& from, const Vec3& target, double fallback)
    {
        if (!finite(from) || !finite(target))
        {
            return wrapYaw(fallback);
        }
        const double dx = target.x - from.x;
        const double dy = target.y - from.y;
        if (dx * dx + dy * dy < 1e-12)
        {
            return wrapYaw(fallback);
        }
        return std::atan2(dy, dx);
    }

    double distance(const Vec3& lhs, const Vec3& rhs)
    {
        if (!finite(lhs) || !finite(rhs))
        {
            return 0.0;
        }
        return length(lhs - rhs);
    }

    double distanceXY(const Vec3& lhs, const Vec3& rhs)
    {
        if (!finite(lhs) || !finite(rhs))
        {
            return 0.0;
        }
        const double dx = lhs.x - rhs.x;
        const double dy = lhs.y - rhs.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    Vec3 clampOffsetXY(const Vec3& value)
    {
        if (!finite(value))
        {
            return Vec3();
        }
        const double magnitude = std::sqrt(value.x * value.x +
                                           value.y * value.y);
        if (magnitude > MAX_OFFSET_METERS)
        {
            const double scale = MAX_OFFSET_METERS / magnitude;
            return Vec3(value.x * scale, value.y * scale, 0.0);
        }
        return Vec3(value.x, value.y, 0.0);
    }

    Vec3 clampPinOffset(const Vec3& value)
    {
        Vec3 result = clampOffsetXY(value);
        result.z = finite(value)
            ? std::clamp(value.z, -MAX_OFFSET_METERS, MAX_OFFSET_METERS) : 0.0;
        return result;
    }

    Vec3 makePinTarget(const Vec3& requested_position, const Vec3& anchor,
                       double visible_z_offset)
    {
        Vec3 relative = clampPinOffset(requested_position - anchor);
        relative.z = clampFinite(visible_z_offset, -MAX_OFFSET_METERS,
                                 MAX_OFFSET_METERS, 0.0);
        return Vec3(anchor.x + relative.x, anchor.y + relative.y, relative.z);
    }

    Vec3 pinTargetOffset(const Vec3& pin, const Vec3& simulated_position)
    {
        Vec3 relative = pin - simulated_position;
        relative.z = pin.z;
        return clampPinOffset(relative);
    }

    Vec3 localOffsetToWorld(const Vec3& value, double world_yaw)
    {
        if (!finite(value) || !finite(world_yaw)) return Vec3();
        const double cosine = std::cos(world_yaw);
        const double sine = std::sin(world_yaw);
        return Vec3(value.x * cosine - value.y * sine,
                    value.x * sine + value.y * cosine, value.z);
    }

    Vec3 worldOffsetToLocal(const Vec3& value, double local_frame_yaw)
    {
        if (!finite(value) || !finite(local_frame_yaw))
        {
            return Vec3();
        }
        const double cosine = std::cos(local_frame_yaw);
        const double sine = std::sin(local_frame_yaw);
        return Vec3(value.x * cosine + value.y * sine,
                    -value.x * sine + value.y * cosine, value.z);
    }

    bool centroid(const std::vector<Vec3>& points, Vec3& result)
    {
        result = Vec3();
        std::size_t count = 0;
        for (const Vec3& point : points)
        {
            if (!finite(point)) continue;
            result = result + point;
            ++count;
        }
        if (count == 0) return false;
        result = result * (1.0 / static_cast<double>(count));
        return true;
    }

    bool reuseFrozenCentroid(bool from_centroid, bool same_dancer_set,
                             const Vec3& centre,
                             const std::vector<Vec3>& dancer_positions,
                             double maximum_distance)
    {
        if (!from_centroid || !same_dancer_set || !finite(centre) ||
            !finite(maximum_distance) || maximum_distance < 0.0)
        {
            return false;
        }
        for (const Vec3& position : dancer_positions)
        {
            if (finite(position) && distance(centre, position) <= maximum_distance)
            {
                return true;
            }
        }
        return false;
    }

    void snap(ScalarSpring& state, double value)
    {
        state.value = finite(value) ? value : 0.0;
        state.velocity = 0.0;
        state.initialized = true;
    }

    void snap(VectorSpring& state, const Vec3& value)
    {
        state.value = finite(value) ? value : Vec3();
        state.velocity = Vec3();
        state.initialized = true;
    }

    double stepCritical(ScalarSpring& state, double target, double dt,
                        double tau, double max_rate, bool angular)
    {
        target = finite(target) ? target : 0.0;
        dt = safeDt(dt);
        tau = clampFinite(tau, 0.001, 60.0, TAU_SECONDS);
        max_rate = clampFinite(max_rate, 0.0, 1e6, 0.0);
        if (!state.initialized || !finite(state.value) ||
            !finite(state.velocity))
        {
            snap(state, target);
            return state.value;
        }
        if (dt <= 0.0)
        {
            return state.value;
        }

        const double adjusted_target = angular
            ? state.value + shortestArc(state.value, target) : target;
        const double omega = 2.0 / tau;
        const double error = state.value - adjusted_target;
        const double helper = state.velocity + omega * error;
        const double decay = std::exp(-omega * dt);
        double next = adjusted_target + (error + helper * dt) * decay;
        double next_velocity = (state.velocity - omega * helper * dt) * decay;

        const double max_step = max_rate * dt;
        const double step = next - state.value;
        if (max_rate > 0.0 && std::fabs(step) > max_step)
        {
            next = state.value + std::copysign(max_step, step);
            next_velocity = std::copysign(max_rate, step);
        }
        state.value = angular ? wrapYaw(next) : next;
        state.velocity = std::clamp(next_velocity, -max_rate, max_rate);
        const double remaining = angular
            ? std::fabs(shortestArc(state.value, target))
            : std::fabs(state.value - target);
        if (remaining < 1e-6 && std::fabs(state.velocity) < 1e-5)
        {
            snap(state, angular ? wrapYaw(target) : target);
        }
        return state.value;
    }

    Vec3 stepCritical(VectorSpring& state, const Vec3& target, double dt,
                      double tau, double max_rate)
    {
        const Vec3 safe_target = finite(target) ? target : Vec3();
        dt = safeDt(dt);
        tau = clampFinite(tau, 0.001, 60.0, TAU_SECONDS);
        max_rate = clampFinite(max_rate, 0.0, 1e6, 0.0);
        if (!state.initialized || !finite(state.value) ||
            !finite(state.velocity))
        {
            snap(state, safe_target);
            return state.value;
        }
        if (dt <= 0.0)
        {
            return state.value;
        }

        const double omega = 2.0 / tau;
        const Vec3 error = state.value - safe_target;
        const Vec3 helper = state.velocity + error * omega;
        const double decay = std::exp(-omega * dt);
        Vec3 next = safe_target + (error + helper * dt) * decay;
        Vec3 next_velocity =
            (state.velocity - helper * (omega * dt)) * decay;
        const Vec3 displacement = next - state.value;
        const double step_length = length(displacement);
        const double max_step = max_rate * dt;
        if (max_rate > 0.0 && step_length > max_step)
        {
            next = state.value + displacement * (max_step / step_length);
            next_velocity = displacement * (max_rate / step_length);
        }
        const double velocity_length = length(next_velocity);
        if (max_rate > 0.0 && velocity_length > max_rate)
        {
            next_velocity = next_velocity * (max_rate / velocity_length);
        }
        state.value = next;
        state.velocity = next_velocity;
        if (distance(state.value, safe_target) < 1e-6 &&
            length(state.velocity) < 1e-5)
        {
            snap(state, safe_target);
        }
        return state.value;
    }

    std::vector<double> pairingYaws(const std::vector<Vec3>& feet,
                                     const Vec3& centre)
    {
        std::vector<double> result(feet.size(), 0.0);
        for (std::size_t i = 0; i + 1 < feet.size(); i += 2)
        {
            result[i] = faceYaw(feet[i], feet[i + 1]);
            result[i + 1] = faceYaw(feet[i + 1], feet[i]);
        }
        if ((feet.size() & 1U) != 0U)
        {
            result.back() = faceYaw(feet.back(), centre);
        }
        return result;
    }

    std::vector<FormationSlot> formationSlots(Formation formation,
                                               std::size_t count,
                                               double radius_or_spacing,
                                               double arc_radians)
    {
        std::vector<FormationSlot> result;
        result.reserve(count);
        if (count == 0)
        {
            return result;
        }
        const double spacing = clampFinite(
            radius_or_spacing, 0.0, MAX_OFFSET_METERS, 0.0);
        arc_radians = clampFinite(arc_radians, -2.0 * PI, 2.0 * PI, PI);
        for (std::size_t i = 0; i < count; ++i)
        {
            FormationSlot slot;
            if (formation == Formation::CIRCLE_IN ||
                formation == Formation::CIRCLE_OUT)
            {
                const double angle = 2.0 * PI * static_cast<double>(i) /
                                     static_cast<double>(count);
                slot.foot = Vec3(std::cos(angle) * spacing,
                                 std::sin(angle) * spacing, 0.0);
                slot.yaw = wrapYaw(angle +
                    (formation == Formation::CIRCLE_IN ? PI : 0.0));
            }
            else if (formation == Formation::ARC)
            {
                const double t = count == 1 ? 0.5 :
                    static_cast<double>(i) / static_cast<double>(count - 1);
                const double angle = -0.5 * arc_radians + arc_radians * t;
                slot.foot = Vec3(std::cos(angle) * spacing,
                                 std::sin(angle) * spacing, 0.0);
                slot.yaw = wrapYaw(angle + PI);
            }
            else if (formation == Formation::LINE)
            {
                slot.foot = Vec3(0.0,
                    (static_cast<double>(i) -
                     0.5 * static_cast<double>(count - 1)) * spacing, 0.0);
                slot.yaw = 0.0;
            }
            else
            {
                const std::size_t columns = static_cast<std::size_t>(
                    std::ceil(std::sqrt(static_cast<double>(count))));
                const std::size_t rows = (count + columns - 1) / columns;
                const std::size_t row = i / columns;
                const std::size_t column = i % columns;
                const std::size_t row_count = std::min(columns,
                    count - row * columns);
                slot.foot = Vec3(
                    (static_cast<double>(row) -
                     0.5 * static_cast<double>(rows - 1)) * spacing,
                    (static_cast<double>(column) -
                     0.5 * static_cast<double>(row_count - 1)) * spacing,
                    0.0);
                slot.yaw = 0.0;
            }
            result.push_back(slot);
        }
        return result;
    }

    bool updatePinState(bool requested, bool currently_pinned,
                        const Vec3& simulated_position, const Vec3& anchor)
    {
        if (!requested || !finite(simulated_position) || !finite(anchor))
        {
            return false;
        }
        const double separation = distanceXY(simulated_position, anchor);
        return currently_pinned ? separation <= PIN_RELEASE_METERS
                                : separation <= PIN_REPIN_METERS;
    }

    bool isDiscontinuity(const Vec3& previous, const Vec3& current)
    {
        return finite(previous) && finite(current) &&
               distance(previous, current) > DISCONTINUITY_METERS;
    }
}

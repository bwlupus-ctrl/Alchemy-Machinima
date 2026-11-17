/** Generic keyframe data and math. No viewer types, clocks or global state. */
#ifndef AL_KEYFRAME_CORE_H
#define AL_KEYFRAME_CORE_H

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ALKf
{
constexpr double pi = 3.14159265358979323846;
constexpr double degree = pi / 180.;
struct Vec3
{
    double x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 b) const;
    Vec3 operator-(Vec3 b) const;
    Vec3 operator-() const;
    Vec3 operator*(double s) const;
    Vec3 operator/(double s) const;
    bool operator==(const Vec3&) const = default;
};
double dot(Vec3, Vec3);
Vec3 cross(Vec3, Vec3);
double length(Vec3);
Vec3 unit(Vec3, Vec3 fallback = {1, 0, 0});
bool finite(Vec3);
struct Quat
{
    double w = 1, x = 0, y = 0, z = 0;
    Quat operator*(Quat) const;
    Quat operator-() const;
    Quat inverse() const;
    Vec3 rotate(Vec3) const;
    bool operator==(const Quat&) const = default;
};
double dot(Quat, Quat);
Quat unit(Quat);
bool finite(Quat);
Quat exp(Vec3 halfAngle);
Vec3 log(Quat);
Quat axisAngle(Vec3 axis, double angle);
Quat canonical(Quat q, Quat predecessor);
Quat slerp(Quat, Quat, double);
double angle(Quat, Quat);
Quat shortestArc(Vec3, Vec3);

struct Doc
{
    using Array = std::vector<Doc>;
    using Object = std::map<std::string, Doc>;
    std::variant<std::monostate, bool, double, std::string, Array, Object> value;
    Doc() = default;
    template<class T> explicit Doc(T v) : value(std::move(v)) {}
    bool operator==(const Doc&) const = default;
};
using Id = std::array<std::uint8_t, 16>;
enum class RefKind { None, Avatar, Clone, Object, Point };
struct RefSlot
{
    RefKind kind = RefKind::None;
    Id id{}, stableId{};
    std::int32_t joint = 0;
    Vec3 fallback;
    std::string label;
    bool operator==(const RefSlot&) const = default;
};
enum class AimMode { FollowLock, OwnTarget, Laid };
struct AimSpec { AimMode mode = AimMode::Laid; RefSlot target; bool operator==(const AimSpec&) const = default; };
enum class ChannelType { Scalar, Vec3, Quat, Step };
enum class Interp { Smooth, Linear, Step };
enum class Space { Linear, LogTan, Angle };
struct Channel
{
    std::string name;
    ChannelType type = ChannelType::Scalar;
    Interp interp = Interp::Smooth;
    Space space = Space::Linear;
    bool operator==(const Channel&) const = default;
};
using Value = std::variant<double, Vec3, Quat, std::int64_t>;
enum class PaceMode { Duration, Speed };
struct SegmentPace { PaceMode mode = PaceMode::Duration; double value = 3; bool operator==(const SegmentPace&) const = default; };
enum class HandleMode { Auto, Aligned, Free };
struct Handles
{
    HandleMode mode = HandleMode::Auto;
    Vec3 in, out; // offsets from the spatial node
    bool operator==(const Handles&) const = default;
};
struct Key
{
    std::uint32_t id = 0;
    double hold = 0;
    SegmentPace out;
    std::vector<Value> values;
    Handles handles;
    bool ease = false;
    AimSpec aim;
    bool operator==(const Key&) const = default;
};
enum class Path { Smooth, Straight };
struct Settings
{
    Path path = Path::Smooth;
    bool evenSpeed = false, easeEnds = true;
    double total = 0, easeSeconds = .6;
    bool operator==(const Settings&) const = default;
};
struct Track
{
    std::uint32_t id = 0, nextKeyId = 1;
    std::string uid, kind = "camera";
    std::vector<Channel> schema;
    std::vector<Key> keys;
    Settings settings;
    RefSlot lock;
    Doc clientPayload;
    std::uint64_t revision = 0;
    bool operator==(const Track&) const = default;
};
struct Sequence
{
    std::string name, uid;
    std::uint32_t formatVersion = 1, nextTrackId = 1;
    bool loop = false;
    double startDelay = 2;
    std::vector<Track> tracks;
    std::uint64_t revision = 0;
    bool operator==(const Sequence&) const = default;
};
// Retained editor sessions, oldest first. The panel keeps at most `cap`; over the cap the oldest
// DISPOSABLE sessions (saved to the library or empty) are dropped. A session with unsaved
// changes is never dropped: the count still over the cap is returned so the UI can say so.
struct SessionSlot { std::string id; Sequence seq; };
std::size_t pruneSessions(std::vector<SessionSlot>&, std::size_t cap,
                          const std::function<bool(const Sequence&)>& disposable, std::vector<std::string>& dropped);
Track cameraTrack();
std::uint32_t appendTrack(Sequence&, Track); // refuses a second camera; 0 on refusal
std::uint32_t appendKey(Sequence&, std::uint32_t track, Key);
bool eraseKey(Sequence&, std::uint32_t track, std::uint32_t key);
bool validate(const Sequence&, std::string& reason);

struct Bezier
{
    std::array<Vec3, 4> p{};
    Vec3 at(double u) const;
    Vec3 derivative(double u) const;
};
struct ArcSample { double u, s; };
struct Arc
{
    Bezier curve;
    std::vector<ArcSample> table;
    double total = 0;
    bool capped = false;
    void build(Bezier);
    double distance(double u) const;
    double inverse(double s) const;
};
struct Timing { double duration = 0, window = 0, cruise = 0; };
Timing durationTiming(double length, double duration, unsigned easedEnds, double ease);
Timing speedTiming(double length, double speed, unsigned easedEnds, double ease);
struct Knot { double t = 0, s = 0, slope = 0; };
double pchip(double hLeft, double hRight, double dLeft, double dRight);
double hermite(double a, double b, double ma, double mb, double h, double u);
double hermiteSlope(double a, double b, double ma, double mb, double h, double u);
struct RotationSegment
{
    std::array<Quat, 4> q;
    bool highAngle = false;
    Quat at(double u) const;
};
struct CachedChannel
{
    std::vector<double> scalar, slope;
    std::vector<Quat> quaternion;
    std::vector<RotationSegment> rotation;
    std::vector<Arc> spatial;
};
struct Cache
{
    std::uint64_t revision = 0;
    double end = 0;
    int position = -1, rotation = -1, fov = -1, roll = -1;
    std::vector<double> arrival, departure, distance;
    std::vector<bool> eased;
    std::vector<Timing> timing;
    std::vector<Knot> knots;
    std::vector<CachedChannel> channels;
    double distanceAt(double t) const;
    double speedAt(double t) const;
};
bool buildCache(const Track&, Cache& result, std::string& reason); // atomic, validates
bool editTotal(Track&, double total, std::string& reason); // atomic scalar bisection edit
double sequenceEnd(const std::vector<Cache>&);
Value evalChannel(const Track&, const Cache&, std::size_t channel, double t);
struct Pose { Vec3 position; Quat rotation; double fov = 60 * degree, roll = 0; };
class Resolver
{
public:
    virtual ~Resolver() = default;
    virtual bool resolve(const RefSlot&, Vec3& global) const = 0;
};
struct Smoother
{
    Vec3 filtered, previous;
    bool initialized = false, present = false;
    Vec3 update(Vec3 sample, bool found, double wallDt);
};
Quat levelAim(Vec3 forward, double roll);
Quat previewAim(Vec3 camera, Vec3 target, Quat laid, double roll);
struct AimState
{
    Quat presented;
    bool initialized = false;
    Quat lastAuthored;         // authored (laid) orientation of the previous Play frame
    bool nearLast = false;     // previous frame was inside the 5 cm laid/aim blend zone
    double integratedTime = 0; // last call; diagnostics for the exact dt contract
    Quat update(Vec3 forward, double roll, double wallDt);
};
enum class EvalMode { Preview, Play };
struct RefIdentity
{
    RefKind kind = RefKind::None;
    Id id{}, stableId{};
    std::int32_t joint = 0;
    Vec3 point;
    explicit RefIdentity(const RefSlot& r) : kind(r.kind), id(r.id), stableId(r.stableId), joint(r.joint),
        point(r.kind==RefKind::Point?r.fallback:Vec3{}) {}
    RefIdentity() = default;
    bool operator==(const RefIdentity&) const = default;
};
struct SourceRuntime { RefIdentity ref; bool assigned = false; Smoother smoother; AimState aim; };
struct ClientRuntime
{
    std::array<SourceRuntime, 2> sources;
    double wallDelta = 0;
    void reset() { *this = {}; }
};
Pose evalCamera(const Track&, const Cache&, double t, const Resolver&, EvalMode, ClientRuntime&);
// Runtime is caller-owned and deliberately absent from the codec and history.
class Client
{
public:
    virtual ~Client() = default;
    virtual const char* kind() const = 0;
    virtual std::vector<Channel> schema() const = 0;
    virtual std::unique_ptr<ClientRuntime> createRuntime() const = 0;
    virtual Key capture() const = 0;
    virtual void apply(const Track&, const Cache&, double, ClientRuntime&) = 0;
    virtual bool restricted(std::string& why) const = 0;
    virtual void rewriteRefs(Sequence&, const std::function<void(RefSlot&)>&) const = 0;
    virtual bool ownsCamera() const = 0;
};
void rewriteRefs(Sequence&, const std::function<void(RefSlot&)>&);
struct HistoryStep
{
    std::shared_ptr<const Sequence> before, after;
    std::string label;
    std::uint64_t seq = 0;
};
class History
{
public:
    void commit(const Sequence& before, const Sequence& after, std::string label, std::uint64_t seq);
    std::uint64_t sequence(bool redo) const;
    bool ready(bool redo, std::uint64_t seq, const Sequence&, bool ownsCamera) const;
    bool replay(bool redo, Sequence&, std::uint64_t replaySeq = 0);
    void discardRedo();
    void discardStep(bool redo, std::uint64_t seq);
    void beginGesture(const Sequence&);
    bool cancelGesture(Sequence&);
    void endGesture();
    void rekey(Sequence&, const std::function<void(RefSlot&)>&);
    std::size_t size(bool redo) const;
private:
    std::vector<HistoryStep> mUndo, mRedo;
    std::shared_ptr<const Sequence> mGesture;
    std::uint64_t mExpectedRevision = 0, mRevisionClock = 0;
    std::uint32_t mNextTrackId = 1;
    std::map<std::uint32_t, std::uint32_t> mNextKeys;
    void observe(const Sequence&);
};
Doc encode(const Sequence&);
bool decode(const Doc&, Sequence& destination, std::string& reason); // validate before swap
} // namespace ALKf

using KfVec3d = ALKf::Vec3;
using KfDoc = ALKf::Doc;
using ALKfClient = ALKf::Client;
using ALKfClientRuntime = ALKf::ClientRuntime;
#endif

/** Private video-file policy. Pure logic: no viewer state or filesystem calls. */
#ifndef AL_VCAM_MEDIA_FILE_POLICY_H
#define AL_VCAM_MEDIA_FILE_POLICY_H

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <string>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <system_error>

namespace ALMediaFile
{
enum class GateSeek { NONE, SEEK, REOPEN_ZERO };
inline GateSeek gateSeekPlan(double target, bool seekable)
{
    if (!std::isfinite(target) || target < 0.) return GateSeek::NONE;
    return seekable ? GateSeek::SEEK : target == 0. ? GateSeek::REOPEN_ZERO : GateSeek::NONE;
}

template<class Seek, class Reopen>
GateSeek applyGateSeek(double target, bool seekable, Seek seek, Reopen reopen)
{
    const auto plan = gateSeekPlan(target, seekable);
    if (plan == GateSeek::SEEK) seek(target);
    else if (plan == GateSeek::REOPEN_ZERO) reopen();
    return plan;
}
inline constexpr const char* UNSUPPORTED = "Only video files (.mp4, .mov, .m4v, .webm, .mkv) are supported";
inline constexpr const char* REMOTE = "Network-share paths are not allowed. Copy the file to a local drive.";
inline constexpr const char* NOT_VIDEO = "This file isn't a video";
inline constexpr const char* MISSING = "File not found";
inline constexpr const char* LOCKED = "Video downloads are in use by another open viewer";
inline constexpr const char* BUDGET = "Budget limited - press Play";
inline constexpr const char* TOO_LARGE = "File too large";
inline constexpr const char* NO_SPACE = "Not enough disk space";
inline constexpr const char* CLEARING = "Clearing video cache...";
inline constexpr const char* CLEAR_DEGRADED = "Some cached videos couldn't be removed";
inline constexpr const char* CLEAR_TIMEOUT = "Video cache clear timed out";
inline constexpr const char* FAILED = "Download failed";
inline constexpr const char* CACHE_UNAVAILABLE = "Video cache unavailable: ";
enum class CacheFailure { IO, IN_USE, NO_SPACE, ACCESS, LOCAL_ONLY };
inline std::string cacheFailureStatus(CacheFailure failure)
{
    if (failure == CacheFailure::IN_USE) return LOCKED;
    const char* reason = failure == CacheFailure::NO_SPACE ? "not enough disk space" :
        failure == CacheFailure::ACCESS ? "access denied" :
        failure == CacheFailure::LOCAL_ONLY ? "local cache required" : "cache I/O failed";
    return std::string(CACHE_UNAVAILABLE) + reason;
}
inline bool mediaRuntimeFailure(const std::string& reason)
{
    if (reason.rfind(CACHE_UNAVAILABLE, 0) == 0 || reason.rfind(FAILED, 0) == 0 ||
        reason.rfind("Unsupported player", 0) == 0 || reason.rfind("Copy failed:", 0) == 0) return true;
    for (const char* failure : {NOT_VIDEO, MISSING, LOCKED, BUDGET, TOO_LARGE, NO_SPACE, CLEAR_TIMEOUT,
        UNSUPPORTED, REMOTE, "Media slots disabled", "Blocked by media policy", "Source missing or unbound"})
        if (reason == failure) return true;
    return false;
}
inline uint64_t nextMediaStatusSequence()
{ static std::atomic<uint64_t> sequence{0}; return ++sequence; }
inline std::string mediaSourceStatus(const std::string& runtime, const std::string& editor,
    uint64_t runtimeSequence = 0, uint64_t editorSequence = 0)
{ return mediaRuntimeFailure(runtime) && runtimeSequence >= editorSequence ? runtime : editor; }
inline CacheFailure cacheFailureFromError(const std::error_code& error, CacheFailure fallback)
{
    // A semantic lock refusal wins over MSVC's permission_denied mapping of
    // ERROR_SHARING_VIOLATION. Preserve the exact second-viewer status.
    if (fallback == CacheFailure::IN_USE) return fallback;
    // Compare conditions through their category; errno EIO and Win32 access
    // denied both have value 5, but describe different failures.
    if (error == std::errc::permission_denied || error == std::errc::operation_not_permitted) return CacheFailure::ACCESS;
    if (error == std::errc::no_space_on_device) return CacheFailure::NO_SPACE;
    return fallback;
}
inline constexpr const char* UNKNOWN_LOOP = "This file has no recorded length - loops may hitch (remux to fix)";
// Keep the public status contract, but preserve the exact rejecting rule for
// diagnostics and the offline probe. Rules are constants, never source text.
struct PolicyRefusal : std::runtime_error
{
    std::string rule;
    explicit PolicyRefusal(const std::string& rule_) : std::runtime_error(NOT_VIDEO), rule(rule_) {}
};
inline std::string diagnosticReason(const std::exception& error)
{
    if (const auto* refusal = dynamic_cast<const PolicyRefusal*>(&error)) return refusal->rule;
    if (std::string(error.what()).rfind(CACHE_UNAVAILABLE, 0) == 0) return error.what();
    for (const char* known : {UNSUPPORTED, REMOTE, NOT_VIDEO, MISSING, LOCKED, BUDGET, TOO_LARGE, NO_SPACE, FAILED})
        if (std::string(error.what()) == known) return known;
    return "I/O or transfer exception"; // Never echo exception text containing a URL/proxy/path.
}
std::string cacheKey(const std::string& url);
enum class Location { HTTP_URL, LOCAL_FILE };
enum class Transport { STOPPED, PAUSED, PLAYING };
enum class Verdict { PENDING, ADMITTED, REJECTED };
struct Descriptor
{
    Location location = Location::HTTP_URL;
    std::string value;
    bool loop = true;
    float volume = 0.f;
};
inline std::string lower(std::string s)
{
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
inline bool control(const std::string& s)
{
    return std::any_of(s.begin(), s.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
inline int hex(char c)
{
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
inline bool decode(std::string s, std::string& out)
{
    out.clear();
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] != '%') out += s[i];
        else
        {
            if (i + 2 >= s.size() || hex(s[i+1]) < 0 || hex(s[i+2]) < 0) return false;
            out += static_cast<char>(hex(s[i+1]) * 16 + hex(s[i+2]));
            i += 2;
        }
    }
    return !control(out);
}
inline std::string extension(const std::string& path)
{
    const auto p = path.find_last_of('.');
    const std::string e = p == std::string::npos ? "" : lower(path.substr(p + 1));
    return e == "mp4" || e == "mov" || e == "m4v" || e == "webm" || e == "mkv" ? e : "";
}
inline std::string diagnosticExtension(const std::string& value)
{
    // Keep only an allowlisted extension. Local '#'/'?' characters are literal;
    // HTTP queries/fragments and file URI percent encoding follow their syntax.
    std::string path = value;
    const auto scheme = lower(value.substr(0, value.find(':')));
    if (scheme == "http" || scheme == "https") path = value.substr(0, value.find_first_of("?#"));
    else if (scheme == "file" && !decode(value.substr(5), path)) return "unsupported";
    const auto ext = extension(path);
    return ext.empty() ? "unsupported" : ext;
}
inline bool networkSpelling(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path.rfind("//", 0) == 0;
}
struct Url { std::string origin, path, request; };
inline bool ipv6(const std::string& host)
{
    if (host.empty() || host.size() > 45 || host.find(":::") != std::string::npos) return false;
    const auto compressed = host.find("::");
    if (compressed != std::string::npos && host.find("::", compressed+2) != std::string::npos) return false;
    if ((host.front() == ':' && host.rfind("::",0) != 0) ||
        (host.back() == ':' && host.substr(host.size()-2) != "::")) return false;
    unsigned groups = 0;
    for (size_t start = 0; start < host.size();)
    {
        const auto end = host.find(':',start);
        const auto part = host.substr(start,end-start);
        if (part.find('.') != std::string::npos)
        {
            if (end != std::string::npos) return false;
            unsigned octets = 0;
            for (size_t p = 0; p < part.size();)
            {
                const auto dot = part.find('.',p);
                const auto octet = part.substr(p,dot-p);
                if (octet.empty() || octet.size() > 3 || octet.find_first_not_of("0123456789") != std::string::npos || std::stoul(octet) > 255) return false;
                ++octets;
                if (dot == std::string::npos) break;
                p = dot+1; if (p == part.size()) return false;
            }
            if (octets != 4) return false;
            groups += 2;
        }
        else if (!part.empty())
        {
            if (part.size() > 4 || part.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return false;
            ++groups;
        }
        if (end == std::string::npos) break;
        start = end+1;
    }
    return compressed == std::string::npos ? groups == 8 : groups < 8;
}
inline bool httpUrl(const std::string& input, Url& out)
{
    if (control(input) || input.find('\\') != std::string::npos || input.find(' ') != std::string::npos) return false;
    const auto colon = input.find("://");
    if (colon == std::string::npos) return false;
    const auto scheme = lower(input.substr(0, colon));
    if (scheme != "http" && scheme != "https") return false;
    const auto end = input.find_first_of("/?#", colon + 3);
    const auto authority = input.substr(colon + 3, end - (colon + 3));
    if (authority.empty() || authority.find_first_of("@%") != std::string::npos) return false;
    std::string host, port;
    if (authority[0] == '[')
    {
        const auto close = authority.find(']');
        if (close == std::string::npos || close < 3) return false;
        host = authority.substr(1, close - 1);
        if (!ipv6(host)) return false;
        if (close + 1 < authority.size())
        {
            if (authority[close+1] != ':') return false;
            port = authority.substr(close + 2);
            if (port.empty()) return false;
        }
    }
    else
    {
        const auto p = authority.find(':');
        host = authority.substr(0, p);
        if (host.empty() || host.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.") != std::string::npos) return false;
        const auto dns = host.back() == '.' ? host.substr(0, host.size()-1) : host;
        if (dns.empty() || dns.size() > 253) return false;
        for (size_t start = 0; start < dns.size();)
        {
            const auto stop = dns.find('.', start);
            const auto label = dns.substr(start, stop - start);
            if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') return false;
            if (stop == std::string::npos) break;
            start = stop + 1;
        }
        if (p != std::string::npos) { port = authority.substr(p+1); if (port.empty()) return false; }
    }
    if (!port.empty())
    {
        if (port.size() > 5 || port.find_first_not_of("0123456789") != std::string::npos) return false;
        const auto n = std::stoul(port);
        if (!n || n > 65535) return false;
    }
    out.origin = input.substr(0, colon + 3) + authority;
    out.request = input.substr(0, input.find('#'));
    const auto pathEnd = input.find_first_of("?#", end);
    out.path = end == std::string::npos || input[end] != '/' ? "/" : input.substr(end, pathEnd - end);
    std::string decoded;
    return decode(out.path, decoded) && !networkSpelling(decoded) && decoded.find('\\') == std::string::npos;
}
inline bool classify(const std::string& input, Descriptor& out, std::string& reason, bool windows)
{
    reason = UNSUPPORTED;
    if (input.empty()) { out.value.clear(); reason.clear(); return true; }
    std::string path = input;
    if (control(path)) return false;
    if (networkSpelling(path)) { reason = REMOTE; return false; }
    const bool drive = path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' && (path[2] == '/' || path[2] == '\\');
    if (lower(path.substr(0, 5)) == "file:")
    {
        path.erase(0, 5);
        if (path.rfind("//", 0) == 0)
        {
            if (path.size() < 3 || path[2] != '/') { reason = REMOTE; return false; }
            path.erase(0, 2); // Empty authority only. Decode exactly once.
        }
        if (!decode(path, path)) return false;
        if (windows && path.size() > 3 && path[0] == '/' && path[2] == ':') path.erase(0, 1);
        if (networkSpelling(path)) { reason = REMOTE; return false; }
    }
    else if (!drive && path[0] != '/')
    {
        Url url;
        if (!httpUrl(path, url) || extension(url.path).empty()) return false;
        out.location = Location::HTTP_URL; out.value = input; reason.clear(); return true;
    }
    if (networkSpelling(path)) { reason = REMOTE; return false; }
    if (windows)
    {
        if (path.size() < 3 || !std::isalpha(static_cast<unsigned char>(path[0])) || path[1] != ':' || (path[2] != '/' && path[2] != '\\')) return false;
        // Reject alternate streams and ambiguous Win32 trailing-dot/space aliases.
        std::replace(path.begin(), path.end(), '\\', '/');
        if (path.find(':', 2) != std::string::npos) return false;
        size_t start = 3;
        std::vector<std::string> parts;
        while (start <= path.size())
        {
            const auto end = path.find('/', start);
            const auto part = path.substr(start, end - start);
            if (part == "..") { if (parts.empty()) return false; parts.pop_back(); }
            else if (!part.empty() && part != ".")
            {
                if (part.back() == '.' || part.back() == ' ' || part.find_first_of("<>\"|?*") != std::string::npos) return false;
                const auto name = lower(part.substr(0, part.find('.')));
                if (name == "con" || name == "nul" || name == "prn" || name == "aux" ||
                    (name.size() == 4 && (name.substr(0,3) == "com" || name.substr(0,3) == "lpt") && name[3] >= '1' && name[3] <= '9')) { reason = REMOTE; return false; }
                parts.push_back(part);
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
        path = path.substr(0, 3);
        for (size_t i = 0; i < parts.size(); ++i) { if (i) path += '/'; path += parts[i]; }
    }
    else
    {
        if (path.empty() || path[0] != '/') return false;
        std::vector<std::string> parts;
        for (size_t start = 1; start <= path.size();)
        {
            const auto end = path.find('/', start);
            const auto part = path.substr(start, end - start);
            if (part == "..") { if (!parts.empty()) parts.pop_back(); }
            else if (!part.empty() && part != ".") parts.push_back(part);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        path.clear();
        for (const auto& part : parts) path += '/' + part;
    }
    if (extension(path).empty()) return false;
    out.location = Location::LOCAL_FILE; out.value = path; reason.clear(); return true;
}
inline std::string fileUri(std::string path)
{
    if (path.empty()) return {};
    if (path.size() > 2 && path[1] == ':') std::replace(path.begin(), path.end(), '\\', '/');
    std::string uri = path[0] == '/' ? "file://" : "file:///";
    const char* digits = "0123456789ABCDEF";
    for (unsigned char c : path)
        if ((std::isalnum(c) && c < 128) || c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~') uri += c;
        else { uri += '%'; uri += digits[c >> 4]; uri += digits[c & 15]; }
    return uri;
}
inline bool redirect(const std::string& from, const std::string& location, unsigned hops, std::string& target)
{
    if (hops >= 5 || location.empty() || control(location) || location.find('\\') != std::string::npos) return false;
    Url base, next;
    if (!httpUrl(from, base)) return false;
    if (location.rfind("//", 0) == 0) target = from.substr(0, from.find(':') + 1) + location;
    else if (location.find(':') != std::string::npos && location.find(':') < location.find_first_of("/?#")) target = location;
    else if (location[0] == '/') target = base.origin + location;
    else if (location[0] == '?') target = base.origin + base.path + location;
    else if (location[0] == '#') target = base.request + location;
    else target = base.origin + base.path.substr(0, base.path.find_last_of('/') + 1) + location;
    return httpUrl(target, next);
}
inline bool mimeAllowed(std::string mime)
{
    mime = lower(mime.substr(0, mime.find(';')));
    while (!mime.empty() && mime.back() == ' ') mime.pop_back();
    if (mime.empty() || mime.back() == '/' || mime.find_first_of(" \t\r\n") != std::string::npos) return false;
    return (mime.rfind("video/", 0) == 0 || mime == "application/octet-stream") &&
        mime.find("mpegurl") == std::string::npos && mime.find("playlist") == std::string::npos && mime.find("dash") == std::string::npos;
}
inline uint64_t be32(const unsigned char* p)
{ return uint64_t(p[0]) << 24 | uint64_t(p[1]) << 16 | uint64_t(p[2]) << 8 | p[3]; }
inline bool sniff(const std::string& ext, const unsigned char* p, size_t n, uint64_t fileSize)
{
    if (ext == "mkv" || ext == "webm") return n >= 5 && p[0] == 0x1a && p[1] == 0x45 && p[2] == 0xdf && p[3] == 0xa3 && p[4] != 0;
    if (n < 8) return false;
    auto size = be32(p);
    const std::string type(reinterpret_cast<const char*>(p + 4), 4);
    uint64_t header = 8;
    if (size == 1) { if (n < 16) return false; size = be32(p+8) << 32 | be32(p+12); header = 16; }
    else if (!size) size = fileSize;
    return size >= header + (type == "ftyp" ? 8u : 0u) && size <= fileSize &&
        (type == "ftyp" || type == "moov" || type == "mdat" || type == "wide" || type == "free");
}
struct Admission
{
    uint64_t token = 0;
    double deadline = 0;
    Verdict verdict = Verdict::PENDING;
    bool seekable = false;
    bool loopReverify = false;
    void open(uint64_t t, double now) { token = t; deadline = now + 10.; verdict = Verdict::PENDING; seekable = false; loopReverify = false; }
    void rebuild(uint64_t t, double now, bool admittedLoop = false)
    {
        const bool retain = admittedLoop && verdict == Verdict::ADMITTED;
        open(t, now);
        loopReverify = retain;
    }
    void resumed(double now) { if (verdict == Verdict::PENDING) deadline = now + 10.; }
    bool observe(uint64_t t, double now, bool video, bool terminal, double length)
    {
        if (t != token || verdict == Verdict::REJECTED) return false;
        if (terminal) verdict = Verdict::REJECTED;
        else if (verdict == Verdict::PENDING)
            verdict = now >= deadline ? Verdict::REJECTED : video ? Verdict::ADMITTED : Verdict::PENDING;
        seekable = verdict == Verdict::ADMITTED && length > 0;
        if (verdict != Verdict::PENDING) loopReverify = false;
        return true;
    }
    bool audible(Transport desired) const { return verdict == Verdict::ADMITTED && desired == Transport::PLAYING; }
    bool retainsOutput() const { return verdict == Verdict::ADMITTED || (verdict == Verdict::PENDING && loopReverify); }
};
// Repeat is configured before input creation. Play-to-end selects false.
// Pure state transitions shared by the callback's atomic fetch_or and
// admission's CAS. A marker on pending state blocks all later admission;
// a marker on admitted state is a normal wrap. Both bits are sticky.
struct WrapPolicy
{
    static constexpr unsigned Admitted = 1u, Marker = 2u;
    static unsigned marker(unsigned state) { return state | Marker; }
    static bool rejects(unsigned state) { return (state & Marker) && !(state & Admitted); }
    static unsigned admit(unsigned state) { return rejects(state) ? state : state | Admitted; }

    double previous = -1.;
    bool stable = false;
    bool timeRegression(double time, double duration, bool pending)
    {
        if (!std::isfinite(time) || time < 0.) { previous = -1.; stable = false; return false; }
        const double drop = previous - time;
        const bool wrap = pending && stable && (drop > 1. || (duration > 0. && drop > duration * .5));
        // Startup can report a stale/non-monotonic first time. Require an
        // actual forward step before treating a regression as an EOF fallback.
        if (previous >= 0. && time > previous) stable = true;
        previous = time;
        return wrap;
    }
};

struct ResumeWait
{
    enum class Action { READY, WAIT, SEEK };
    double position = 0., deadline = 0.;
    bool sent = false;
    double loopTarget(double sampled, bool seekable) const
    { return position > 0. ? position : seekable && sampled > 0. ? sampled : 0.; }
    void open(double target, bool seekable)
    {
        position = seekable && target > 0. ? target : 0.;
        resumed();
    }
    // Paused/opening time must not consume the seek window. A resumed pending
    // replacement gets a fresh seek, armed only when Playing is observed.
    void resumed() { deadline = 0.; sent = false; }
    Action step(double now, bool playing, bool seekFailed, double time)
    {
        if (!(position > 0.)) return Action::READY;
        if (playing && deadline == 0.) deadline = now + .5;
        if (seekFailed || (deadline > 0. && now >= deadline) || (sent && time >= position - .01))
        { position = 0.; return Action::READY; }
        if (!sent && playing) { sent = true; return Action::SEEK; }
        return Action::WAIT;
    }
};

// VLC 3's duration-less Matroska cannot seek; only it may recover by reopen.
inline const char* inputRepeatOption(bool loop) { return loop ? ":input-repeat=65535" : ":input-repeat=0"; }
inline bool unknownLoopRecovery(const Admission& admission, bool loop, Transport desired, double length)
{
    return admission.verdict == Verdict::ADMITTED && loop && desired == Transport::PLAYING && !(length > 0.);
}
inline bool clearDisabledStatus(std::string& reason, bool enabled, bool empty)
{
    if (!enabled || (reason != "Turn on Virtual Cam to play video" && reason != "Media slots disabled")) return false;
    reason = empty ? "Enter a direct video URL" : "Set. Press Enter, Load or Play to start it.";
    return true;
}
inline bool clearPlayStatusOnPolicyChange(std::string& status, bool oldMaster, bool oldSlots, bool master, bool slots)
{
    if (oldMaster == master && oldSlots == slots) return false;
    status.clear();
    return true;
}
// Five sharing-violation attempts spanning two seconds, bounded by the I/O deadline.
inline unsigned lockRetryMs(unsigned failedAttempts, bool sharingViolation)
{ return sharingViolation && failedAttempts < 5 ? 500 : 0; }
inline bool bytesFromMB(int64_t mb, uint64_t& bytes)
{
    if (mb <= 0 || uint64_t(mb) > UINT64_MAX / (1024 * 1024)) return false;
    bytes = uint64_t(mb) * 1024 * 1024; return true;
}
}
#endif

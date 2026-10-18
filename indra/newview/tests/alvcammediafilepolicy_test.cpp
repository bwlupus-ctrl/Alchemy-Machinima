/** Pure private-file gates; no filesystem, network, plugin or viewer fixture. */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alvcammediafilepolicy.h"
#include "../alvcammediafilecontainer.h"
#include "../alvcammediafilelifecycle.h"
#include "../../llplugin/llpluginfileclose.h"
#include "../../llplugin/llpluginfileadmission.h"
#include <array>
#include <cstring>
#include <set>
#include "alvcamfragmentfixture.h"

namespace tut
{
struct media_file_data {};
typedef test_group<media_file_data> media_file_group;
typedef media_file_group::object media_file_object;
media_file_group media_file_tests("alvcammediafilepolicy");
using namespace ALMediaFile;

template<> template<> void media_file_object::test<1>()
{
    Descriptor d; std::string why;
    ensure("empty cue", classify("", d, why, true));
    ensure("default loop", d.loop); ensure_equals("default muted", d.volume, 0.f);
    ensure("HTTP path ignores query and fragment", classify("https://example.test/clip.MP4?x=1#tail", d, why, true));
    ensure("http kind", d.location == Location::HTTP_URL);
    ensure("IPv6 URL", classify("https://[::1]:8443/clip.mp4", d, why, true));
    ensure("IPv4-mapped IPv6", classify("http://[::ffff:192.0.2.1]/clip.mp4", d, why, true));
    ensure("invalid IPv6 refused", !classify("https://[:::]/clip.mp4", d, why, true));
    ensure("native path before URI", classify("D:\\footage\\a#b%20.MOV", d, why, true));
    ensure_equals("native literal chars", d.value, std::string("D:/footage/a#b%20.MOV"));
    ensure("file decode once", classify("file:///D:/a%2520%23b%20clip.mov", d, why, true));
    ensure_equals("decoded once", d.value, std::string("D:/a%20#b clip.mov"));
    ensure_equals("encoded once", fileUri(d.value), std::string("file:///D:/a%2520%23b%20clip.mov"));
    ensure("POSIX", classify("/home/me/a#b%.webm", d, why, false));
    ensure_equals("POSIX backslash is literal", fileUri("/home/me/a\\b.mp4"), std::string("file:///home/me/a%5Cb.mp4"));
    for (const auto* input : {"relative.mp4", "C:clip.mp4", "https://example.test/live.m3u8", "rtsp://example.test/a.mp4", "data:video/mp4", "https://user:pass@example.test/a.mp4", "https://example.test:0/a.mp4", "C:/dir/clip.mp4:stream", "C:/dir/nul.mp4"})
        ensure(std::string("refused ") + input, !classify(input, d, why, true));
}
template<> template<> void media_file_object::test<2>()
{
    Descriptor d; std::string why;
    for (const auto* input : {"\\\\server\\share\\clip.mp4", "//server/share/clip.mp4", "\\\\?\\UNC\\server\\share\\clip.mp4", "\\\\.\\C:\\clip.mp4", "\\\\?\\C:\\clip.mp4", "file://localhost/C:/clip.mp4", "file:////server/share/clip.mp4", "file:///%2fserver/share/clip.mp4", "file:%5c%5cserver/share/clip.mp4", "/\\server/share/clip.mp4"})
    {
        ensure(std::string("lexical refusal ") + input, !classify(input, d, why, true));
        ensure_equals("exact network status", why, std::string(REMOTE));
    }
    ensure("lexical dots normalized", classify("C:/a/../clip.mkv", d, why, true));
    ensure_equals("canonical native", d.value, std::string("C:/clip.mkv"));
}
template<> template<> void media_file_object::test<3>()
{
    std::array<unsigned char,64> header{};
    header[3] = 24; header[4] = 'f'; header[5] = 't'; header[6] = 'y'; header[7] = 'p';
    ensure("BMFF", sniff("mp4", header.data(), 64, 512));
    ensure("truncated box", !sniff("mp4", header.data(), 64, 16));
    ensure("truncated header", !sniff("mp4", header.data(), 7, 512));
    header[3] = 8;
    ensure("ftyp minimum", !sniff("mp4", header.data(), 64, 512));
    header[4]='w'; header[5]='i'; header[6]='d'; header[7]='e';
    ensure("old QuickTime", sniff("mov", header.data(), 64, 512));
    header[0]=0x1a; header[1]=0x45; header[2]=0xdf; header[3]=0xa3; header[4]=0x9f;
    ensure("EBML", sniff("webm", header.data(), 64, 512));
    ensure("wrong container", !sniff("mp4", header.data(), 64, 512));
    ensure("video MIME", mimeAllowed("video/mp4; charset=binary"));
    ensure("octet MIME", mimeAllowed("application/octet-stream"));
    for (const auto* mime : {"", "application/mp4", "binary/octet-stream", "text/html", "audio/mp4", "application/vnd.apple.mpegurl", "video/vnd.mpegurl", "application/dash+xml"}) ensure("MIME refused", !mimeAllowed(mime));
}
template<> template<> void media_file_object::test<4>()
{
    std::string next;
    ensure("relative CDN hop", redirect("https://a.test/folder/a.mp4", "../signed", 0, next));
    ensure("final path need not have extension at hop", next.find("signed") != std::string::npos);
    ensure("cross-host HTTPS", redirect("https://a.test/a.mp4", "//b.test/clip.webm", 4, next));
    ensure("sixth refused", !redirect("https://a.test/a.mp4", "https://b.test/clip.webm", 5, next));
    for (const auto* target : {"file:///C:/clip.mp4", "rtsp://b.test/clip.mp4", "https://user@b.test/a.mp4", "https://b.test:65536/a.mp4", "https://b.test\\evil/a.mp4", "https://b.test/%5c%5cserver/a.mp4", "https://b.test/a.mp4\r\nX: bad"}) ensure("hop refused before request", !redirect("https://a.test/a.mp4", target, 0, next));
}
template<> template<> void media_file_object::test<5>()
{
    ensure_equals("SHA256 known vector", cacheKey("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    ensure_equals("fragment excluded", cacheKey("https://example.test/a.mp4?x=1#one"), cacheKey("https://example.test/a.mp4?x=1#two"));
    ensure("query retained", cacheKey("https://example.test/a.mp4?x=1") != cacheKey("https://example.test/a.mp4?x=2"));
    const auto hash = cacheKey("https://example.test/a.mp4?private-value");
    ensure_equals("only hex", hash.find_first_not_of("0123456789abcdef"), std::string::npos);
}
template<> template<> void media_file_object::test<6>()
{
    Admission a; a.open(7, 100.);
    ensure("pending muted", !a.audible(Transport::PLAYING));
    ensure("audio-first not terminal", a.observe(7, 100.1, false, false, 5.));
    ensure("still pending", a.verdict == Verdict::PENDING);
    ensure("stale video rejected", !a.observe(6, 101., true, false, 5.));
    ensure("lengthless video", a.observe(7, 101., true, false, 0.));
    ensure("admitted", a.verdict == Verdict::ADMITTED);
    ensure("nonseekable", !a.seekable);
    ensure("playing audible", a.audible(Transport::PLAYING));
    ensure("pause zero latch", !a.audible(Transport::PAUSED));
    a.observe(7, 102., true, false, 20.); ensure("length capability", a.seekable);
    a.open(8, 103.); ensure("rebuild muted", !a.audible(Transport::PLAYING));
    a.observe(8, 112.9, false, false, 20.); ensure("deadline not reset", a.deadline == 113.);
    a.observe(8, 113., false, false, 20.); ensure("deadline terminal", a.verdict == Verdict::REJECTED);
    a.observe(8, 114., true, false, 20.); ensure("no recovery", a.verdict == Verdict::REJECTED);
    a.open(9, 120.); a.observe(9, 120.1, false, true, 0.); ensure("early end/error", a.verdict == Verdict::REJECTED);
}
template<> template<> void media_file_object::test<7>()
{
    unsigned elapsed = 0;
    for (unsigned failure = 1; failure <= 5; ++failure) elapsed += lockRetryMs(failure, true);
    ensure_equals("sharing backoff two seconds", elapsed, 2000u);
    ensure_equals("non-sharing terminal", lockRetryMs(1, false), 0u);
    ensure_equals("five attempts bounded", lockRetryMs(5, true), 0u);
    uint64_t bytes = 0;
    ensure("MiB", bytesFromMB(2048, bytes)); ensure_equals("2 GiB", bytes, uint64_t(2147483648));
    ensure("zero rejected", !bytesFromMB(0, bytes)); ensure("negative rejected", !bytesFromMB(-1, bytes));
    ensure("overflow rejected", !bytesFromMB(INT64_MAX, bytes));
}
namespace
{
using Bytes = std::vector<unsigned char>;
Bytes join(Bytes a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); return a; }
Bytes box(const std::string& type, const Bytes& body)
{
    const auto n = body.size() + 8;
    Bytes result{static_cast<unsigned char>(n>>24), static_cast<unsigned char>(n>>16), static_cast<unsigned char>(n>>8), static_cast<unsigned char>(n)};
    result.insert(result.end(), type.begin(), type.end());
    return join(result, body);
}
Bytes ebml(Bytes id, const Bytes& body)
{
    if (body.size() >= 127) throw std::runtime_error("fixture too big");
    id.push_back(static_cast<unsigned char>(0x80 | body.size()));
    return join(id, body);
}
bool container(const Bytes& bytes, const std::string& ext)
{
    try
    {
        checkContainer([&bytes](uint64_t offset, void* out, size_t count)
        {
            if (offset > bytes.size() || count > bytes.size()-offset) throw std::runtime_error("out of bounds");
            std::memcpy(out, bytes.data()+offset, count);
        }, bytes.size(), ext);
        return true;
    }
    catch (const std::exception&) { return false; }
}
Bytes matroska(const Bytes& body)
{ return join(Bytes{0x1a,0x45,0xdf,0xa3,0x80,0x18,0x53,0x80,0x67,0xff}, body); }
}
template<> template<> void media_file_object::test<8>()
{
    const auto ftyp = box("ftyp", Bytes{'i','s','o','m',0,0,0,0});
    auto refs = [](const Bytes& entry)
    {
        return box("moov",box("trak",box("mdia",box("minf",box("dinf",
            box("dref",join(Bytes{0,0,0,0,0,0,0,1}, entry)))))));
    };
    const auto local = refs(box("url ",Bytes{0,0,0,1}));
    const auto encoded = box("mdat",Bytes{'r','m','r','a','d','r','e','f',0x6e,0x67});
    ensure("self-contained dref", container(join(ftyp,local),"mov"));
    ensure("encoded frame markers stay opaque", container(join(join(ftyp,encoded),local),"mp4"));
    for (const std::string target : {"C:/other.mov", "//sentinel/share/other.mov", "https://sentinel.test/other.mov"})
    {
        Bytes payload{0,0,0,0}; payload.insert(payload.end(), target.begin(), target.end()); payload.push_back(0);
        ensure("external dref refused", !container(join(ftyp,refs(box("url ",payload))),"mov"));
    }
    ensure("reference movie refused", !container(join(ftyp,box("moov",box("rmra",{}))),"mov"));
    ensure("compressed reference metadata fails closed", !container(join(ftyp,box("moov",box("cmov",{}))),"mov"));
    auto truncated = join(ftyp,local); truncated.pop_back();
    ensure("truncated structure refused", !container(truncated,"mp4"));
}
template<> template<> void media_file_object::test<9>()
{
    const auto payload = Bytes{0x3c,0xb9,0x23,0x6e,0x67,0x44,0x44};
    const auto cluster = ebml(Bytes{0x1f,0x43,0xb6,0x75},ebml(Bytes{0xa3},payload));
    ensure("encoded MKV markers stay opaque",container(matroska(cluster),"mkv"));
    const auto info = [](Bytes reference) { return ebml(Bytes{0x15,0x49,0xa9,0x66},reference); };
    for (const Bytes id : {Bytes{0x3c,0xb9,0x23},Bytes{0x3e,0xb9,0x23},Bytes{0x3c,0x83,0xab},Bytes{0x3e,0x83,0xbb},Bytes{0x44,0x44}})
        ensure("linked metadata refused", !container(matroska(join(info(ebml(id,Bytes{1})),cluster)),"webm"));
    const auto chapter = ebml(Bytes{0x10,0x43,0xa7,0x70},ebml(Bytes{0x45,0xb9},ebml(Bytes{0xb6},ebml(Bytes{0x6e,0x67},Bytes{1}))));
    ensure("chapter segment reference refused", !container(matroska(join(cluster,chapter)),"mkv"));
    const auto unknownCluster = join(Bytes{0x1f,0x43,0xb6,0x75,0xff},ebml(Bytes{0xa3},payload));
    ensure("duration-less unknown Segment/Cluster",container(matroska(join(unknownCluster,info({}))),"webm"));
    const auto cue = ebml(Bytes{0x1c,0x53,0xbb,0x6b},ebml(Bytes{0xbb},ebml(Bytes{0xb7},ebml(Bytes{0xf1},Bytes{0}))));
    ensure("cue names real Cluster",container(matroska(join(cluster,cue)),"mkv"));
    const auto seek = [](unsigned char position)
    {
        return ebml(Bytes{0x11,0x4d,0x9b,0x74},ebml(Bytes{0x4d,0xbb},join(ebml(Bytes{0x53,0xab},Bytes{0x1f,0x43,0xb6,0x75}),ebml(Bytes{0x53,0xac},Bytes{position}))));
    };
    ensure("real seek target",container(matroska(join(cluster,seek(0))),"mkv"));
    ensure("seek into opaque payload refused",!container(matroska(join(cluster,seek(6))),"mkv"));
}

// Fix round 1: no real clocks, I/O, VLC, threads or viewer required.
template<> template<> void media_file_object::test<10>()
{
    set_test_name("maintenance survives thirty minutes and a timed-out item");
    int64_t now = 31 * 60 * 1000;
    ensure("permanent worker has no total limit", !totalExpired(true, 1, now));
    ensure("transfer still has its limit", totalExpired(false, 1, now));
    ensure("queued transfers do not age", !totalExpired(false, 0, now));
    unsigned reports = 0, retired = 0, cleared = 0;
    const auto report = [&]() { ++reports; };
    maintenanceItem([&]()
    {
        const auto deadline = now + 5000;
        now += 5000;
        finishOperation(true, deadlineExpired(deadline, now), false);
    }, report);
    maintenanceItem([&]() { ++retired; }, report);
    maintenanceItem([&]() { ++cleared; }, report);
    ensure_equals("failed op reports once", reports, 1u);
    ensure_equals("later retirement completes", retired, 1u);
    ensure_equals("later Clear completes", cleared, 1u);
    ensure("transfer timeouts still cancel",finishOperation(false,true,false));
    ensure("unwinding maintenance preserves its original exception",!finishOperation(true,true,true));
    maintenanceItem([]() { throw 7; }, report);
    ensure_equals("nonstandard exception stays inside item", reports, 2u);
}
template<> template<> void media_file_object::test<11>()
{
    set_test_name("deadline check consumes one changing atomic snapshot");
    struct ChangingDeadline
    {
        mutable unsigned loads = 0;
        int64_t load() const { return loads++ == 0 ? 100 : 0; }
    } deadline;
    ensure("a concurrently cleared future deadline cannot time out", !ioExpired(deadline, 90));
    ensure_equals("one atomic load per check", deadline.loads, 1u);
}
template<> template<> void media_file_object::test<12>()
{
    set_test_name("Stop and Clear retain protection until acknowledged close or exit");
    auto file = std::make_shared<int>(1);
    std::weak_ptr<int> held = file;
    auto fence = std::make_shared<LLPluginFileClose>();
    double now = 1.;
    DeferredClose retiring{file, [&]() { return fence->poll(now); }};
    file.reset();
    ensure("Stop is not child close", !retiring.poll() && !held.expired());
    now = 6.;
    ensure("timeout does not allow Clear to unlink", !retiring.poll() && !held.expired());
    ensure("timeout requests fallback termination", fence->force.load());
    // Parent reports this only after stop/release acknowledgement or actual exit.
    fence->closed = true;
    ensure("confirmed close retires the lease", retiring.poll() && held.expired());
}
template<> template<> void media_file_object::test<13>()
{
    set_test_name("iPhone QuickTime meta alias and udta terminator");
    const auto ftyp = box("ftyp", Bytes{'q','t',' ',' ',0,0,0,0});
    const auto refs = [](const std::string& type, const Bytes& flags)
    { return box("dinf",box("dref",join(Bytes{0,0,0,0,0,0,0,1},box(type,flags)))); };
    const auto qtMeta = box("meta",box("hdlr",Bytes(24,0)));
    const auto isoMeta = box("meta",join(Bytes(4,0),box("hdlr",Bytes(24,0))));
    const auto udta = box("udta",join(qtMeta,Bytes(4,0)));
    const auto movie = [&](const Bytes& ref)
    { return join(ftyp,box("moov",join(box("trak",box("mdia",box("minf",ref))),udta))); };
    ensure("synthetic iPhone MOV admitted",container(movie(refs("alis",Bytes{0,0,0,1})),"mov"));
    ensure("ISO FullBox meta admitted",container(join(ftyp,box("moov",isoMeta)),"mp4"));
    ensure("alias without self flag refused",!container(movie(refs("alis",Bytes{0,0,0,0})),"mov"));
    ensure("reference movie refused",!container(join(movie(refs("alis",Bytes{0,0,0,1})),box("rmra",{})),"mov"));
    ensure("external URL refused",!container(movie(refs("url ",Bytes{0,0,0,0,'h','t','t','p',':','/','/',0})),"mov"));
    ensure("self flag cannot hide a URL payload",!container(movie(refs("url ",Bytes{0,0,0,1,'x',0})),"mov"));
    ensure("nonzero udta tail refused",!container(join(ftyp,box("moov",box("udta",Bytes{0,0,0,1}))),"mov"));
    ensure("QT meta still walks references",!container(join(ftyp,box("moov",box("meta",join(box("hdlr",{}),box("rmra",{}))))),"mov"));
}
template<> template<> void media_file_object::test<14>()
{
    set_test_name("launch delay does not consume the fixed ten-second verdict");
    LLPluginFileAdmission clock;
    clock.queued(100.,60.);
    ensure("launch may take more than ten seconds",!clock.expired(125.));
    clock.opened(125.);
    ensure("opens with ten seconds remaining",!clock.expired(134.9));
    clock.opened(134.9);
    ensure("repeated pending cannot extend deadline",clock.expired(135.));
    clock.queued(200.,60.);
    ensure("hung launch bounded by launch allowance",clock.expired(260.));
}
template<> template<> void media_file_object::test<15>()
{
    set_test_name("POSIX admission and Loop compare validated byte identity");
    const FileIdentity validated{1,2,1024,100,200};
    ensure("unchanged descriptor admits and loops",validated == validated);
    for (unsigned field = 0; field < 5; ++field)
    {
        auto changed = validated;
        switch (field)
        {
        case 0: ++changed.device; break;
        case 1: ++changed.inode; break;
        case 2: ++changed.size; break;
        case 3: ++changed.seconds; break;
        default: ++changed.nanos; break;
        }
        ensure("any byte identity change rejects",!(validated == changed));
    }
}
template<> template<> void media_file_object::test<16>()
{
    Descriptor d; std::string why;
    for (const auto* path : {"C:/nul.mp4","C:/CON.mov","C:/aux/a.mp4","C:/COM1.mp4","C:/LPT9.mov"})
    {
        ensure("DOS device refused",!classify(path,d,why,true));
        ensure_equals("device uses exact network/device refusal",why,std::string(REMOTE));
    }
}

template<> template<> void media_file_object::test<17>()
{
    set_test_name("corrupt cache hits become one ordinary miss; transfer failures never retry");
    ensure("missing entry is a miss",retryCacheHit(MISSING));
    ensure("bad size or container is a miss",retryCacheHit(NOT_VIDEO));
    for (const auto* reason : {FAILED,TOO_LARGE,NO_SPACE,REMOTE})
        ensure("other failures are terminal",!retryCacheHit(reason));
}
template<> template<> void media_file_object::test<18>()
{
    set_test_name("minimize and admitted Loop restart preserve private playback");
    ensure("minimized private player keeps budget priority",!windowMinimizedHides(true,false));
    ensure("ordinary minimized media still hides",windowMinimizedHides(false,false));
    ensure("active windows retain priority",!windowMinimizedHides(false,true));
    ensure("DONE during admitted Loop restart cannot stop",!privateEndStops(true,true,true));
    ensure("non-loop EOF still stops",privateEndStops(true,true,false));
    ensure("pending DONE belongs to admission gate",!privateEndStops(false,true,false));
}

template<> template<> void media_file_object::test<19>()
{
    set_test_name("production retirement polls the worker's steady clock");
    auto held = std::make_shared<int>(1);
    std::weak_ptr<int> lease = held;
    auto fence = std::make_shared<LLPluginFileClose>();
    DeferredClose retirement(held, fence); // same constructor as retireAfterClose
    held.reset();
    const double now = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    fence->deadline = now + 60.;
    ensure("future monotonic deadline retains lease", !retirement.poll() && !lease.expired());
    ensure("future monotonic deadline does not terminate", !fence->force.load());
    fence->deadline = now - 1.;
    ensure("past monotonic deadline still retains lease", !retirement.poll() && !lease.expired());
    ensure("production clock requests termination", fence->force.load());
    fence->observeProcess(true, false, []() { return false; }, []() {});
    ensure("observed exit releases production retirement", retirement.poll() && lease.expired());
}

template<> template<> void media_file_object::test<20>()
{
    set_test_name("parent fence never signals reaped PIDs and waits for actual exit");
    LLPluginFileClose reaped;
    reaped.force = true;
    unsigned signals = 0, observations = 0;
    reaped.observeProcess(true, false, [&]() { ++observations; return false; }, [&]() { ++signals; });
    ensure_equals("reaped PID must never receive SIGKILL", signals, 0u);
    ensure_equals("process state checked first", observations, 1u);
    ensure("reaped process confirms close", reaped.closed.load());

    auto fence = std::make_shared<LLPluginFileClose>();
    auto held = std::make_shared<int>(1);
    std::weak_ptr<int> lease = held;
    DeferredClose retirement(held, fence);
    held.reset();
    fence->deadline = -1.; // real worker poll triggers force without sleeping
    ensure("timeout retains protection", !retirement.poll());
    fence->observeProcess(true, false, [&]() { ++observations; return true; }, [&]()
    { ensure_equals("running observation precedes signal", observations, 2u); ++signals; });
    ensure_equals("owned running process receives termination once", signals, 1u);
    ensure("kill request is not close", !retirement.poll() && !lease.expired());
    fence->observeProcess(true, false, []() { return true; }, [&]() { ++signals; });
    ensure_equals("timeout cannot repeatedly signal", signals, 1u);
    fence->observeProcess(true, false, []() { return false; }, [&]() { ++signals; });
    ensure("actual exit closes and releases lease", retirement.poll() && lease.expired());
    LLPluginFileClose launching;
    launching.observeProcess(false, true, []() { return false; }, []() {});
    ensure("pending launch cannot acknowledge close", !launching.closed.load());
    launching.observeProcess(false, false, []() { return false; }, []() {});
    ensure("cancelled launch without child closes", launching.closed.load());
}

template<> template<> void media_file_object::test<21>()
{
    set_test_name("QuickTime padding cannot hide promoted or compressed movie metadata");
    const auto ftyp = box("ftyp", Bytes{'q','t',' ',' ',0,0,0,0});
    const auto meta = box("meta",box("hdlr",Bytes(24,0)));
    const auto movie = box("moov",box("mvhd",Bytes(100,0)));
    ensure("QT meta followed by free movie references refused",
        !container(join(join(ftyp,meta),box("free",join(box("mvhd",Bytes(100,0)),box("rmra",{})))),"mov"));
    ensure("compressed moov refused",!container(join(ftyp,box("moov",box("cmov",{}))),"mov"));
    ensure("zero free padding before valid movie admitted",
        container(join(join(ftyp,box("free",Bytes(128,0))),movie),"mov"));
    ensure("non-box padding admitted",container(join(join(ftyp,box("free",Bytes{1,2,3,4,5,6,7,8,9})),movie),"mov"));
    for (const auto* padding : {"free","skip","wide"})
        for (const auto* metadata : {"moov","mvhd","rmra","rmda","dref","trak","cmov"})
        {
            const auto hidden = box(padding,box(metadata,{}));
            ensure("root movie padding refused",!container(join(ftyp,hidden),"mov"));
            ensure("moov movie padding refused",!container(join(ftyp,box("moov",hidden)),"mov"));
            ensure("nested padding refused",!container(join(ftyp,box("free",hidden)),"mov"));
        }
    const auto alias = box("dinf",box("dref",join(Bytes{0,0,0,0,0,0,0,1},box("alis",Bytes{0,0,0,1}))));
    const auto iphone = box("moov",join(box("trak",box("mdia",box("minf",alias))),box("udta",join(meta,Bytes(4,0)))));
    ensure("iPhone-style MOV remains admitted",container(join(ftyp,iphone),"mov"));
}

template<> template<> void media_file_object::test<22>()
{
    set_test_name("production validation rechecks identity after reading and on reopen");
    const auto bytes = join(box("ftyp",Bytes{'i','s','o','m',0,0,0,0}),box("moov",box("mvhd",Bytes(100,0))));
    const FileIdentity original{1,2,bytes.size(),100,200};
    auto current = original;
    const auto read = [&](uint64_t offset, void* dst, size_t count)
    {
        if (offset > bytes.size() || count > bytes.size() - offset) throw std::runtime_error(NOT_VIDEO);
        std::memcpy(dst,bytes.data()+offset,count);
    };
    ensure("unchanged validated bytes admitted",validateFile(read,[&]() { return current; },"mp4") == original);
    for (unsigned field = 0; field < 5; ++field)
    {
        current = original;
        bool changed = false;
        const auto mutate = [&]()
        {
            switch (field)
            {
            case 0: ++current.device; break;
            case 1: ++current.inode; break;
            case 2: ++current.size; break;
            case 3: ++current.seconds; break;
            default: ++current.nanos; break;
            }
        };
        bool rejected = false;
        try
        {
            validateFile([&](uint64_t offset, void* dst, size_t count)
            { read(offset,dst,count); if (!changed) { mutate(); changed = true; } },
                [&]() { return current; },"mp4");
        }
        catch (const std::runtime_error& e) { rejected = std::string(e.what()) == NOT_VIDEO; }
        ensure("mutation during production walk refuses admission",rejected);
        rejected = false;
        try { validateFile(read,[&]() { return current; },"mp4",&original); }
        catch (const std::runtime_error& e) { rejected = std::string(e.what()) == NOT_VIDEO; }
        ensure("reopen rejects changed validated identity",rejected);
    }
}

template<> template<> void media_file_object::test<23>()
{
    set_test_name("production cache-hit entry validates then evicts corrupt bytes as a miss");
    const auto good = join(box("ftyp",Bytes{'i','s','o','m',0,0,0,0}),box("moov",{}));
    const auto bad = join(box("ftyp",Bytes{'i','s','o','m',0,0,0,0}),box("moov",box("cmov",{})));
    unsigned evictions = 0, publications = 0, gets = 0;
    auto request = [&](const Bytes& bytes)
    {
        const bool hit = validatedCacheHit([&]()
        {
            return validateFile([&](uint64_t offset, void* dst, size_t count)
            { if (offset > bytes.size() || count > bytes.size()-offset) throw std::runtime_error(NOT_VIDEO); std::memcpy(dst,bytes.data()+offset,count); },
                [&]() { return FileIdentity{1,2,bytes.size(),3,4}; },"mp4");
        }, [&](const FileIdentity&) { ++publications; }, [&]() { ++evictions; });
        if (!hit) ++gets;
    };
    request(bad);
    ensure_equals("corrupt hit evicted",evictions,1u);
    ensure_equals("corrupt hit never published",publications,0u);
    ensure_equals("corrupt hit takes ordinary miss once",gets,1u);
    request(good);
    ensure_equals("validated hit published",publications,1u);
    ensure_equals("valid hit adds no GET",gets,1u);
    bool terminal = false;
    try
    {
        validatedCacheHit([]() -> FileIdentity { throw std::runtime_error(FAILED); },
            [&](const FileIdentity&) { ++publications; }, [&]() { ++evictions; });
    }
    catch (const std::runtime_error&) { terminal = true; }
    ensure("transfer/storage failure propagates",terminal);
    ensure_equals("terminal failure cannot evict or retry",evictions,1u);
}

template<> template<> void media_file_object::test<24>()
{
    set_test_name("resume after paused player rebuild starts a fresh viewer verdict window");
    LLPluginFileAdmission clock;
    clock.queued(100.,60.);
    clock.opened(100.); // new player built while paused
    clock.resumed(125.,1,2,0);
    ensure("late resume has full verdict time",!clock.expired(134.9));
    ensure_equals("production verdict remains pending after late resume",clock.verdict(0,2,134.9),0);
    clock.resumed(134.9,2,2,0);
    ensure("duplicate Playing cannot extend verdict",clock.expired(135.));
    ensure_equals("production pending verdict still times out",clock.verdict(0,2,135.),-1);
    clock.resumed(200.,1,2,1);
    ensure_equals("admitted file keeps its admission across resume",clock.verdictDeadline,135.);
}

template<> template<> void media_file_object::test<25>()
{
    set_test_name("successful maintenance clears failure and failed Clear stays gated");
    std::atomic<bool> failed{false}, clear{true}, cleanup{false};
    unsigned leasedEntries = 1;
    maintenanceWork([&]()
    {
        maintainCache(clear,cleanup,[](bool) { throw std::runtime_error(FAILED); },[&]() { return leasedEntries != 0; });
    },failed);
    ensure("failure status visible",failed.load());
    ensure("failed Clear still gates leased hits and schedules retry",clear.load() && cleanup.load());
    maintenanceWork([&]()
    {
        maintainCache(clear,cleanup,[](bool clearing) { if (!clearing) throw std::runtime_error(FAILED); },[&]() { return leasedEntries != 0; });
    },failed);
    ensure("next successful item clears error",!failed.load());
    ensure("successful sweep still gates leased entries",clear.load());
    leasedEntries = 0;
    maintenanceWork([&]() { maintainCache(clear,cleanup,[](bool) {},[&]() { return leasedEntries != 0; }); },failed);
    ensure("Clear completes only after leases drain",!clear.load() && !cleanup.load() && !failed.load());
}

template<> template<> void media_file_object::test<26>()
{
    set_test_name("every replacement player is muted until fresh video verification");
    Admission player;
    player.open(1,100.);
    player.observe(1,101.,true,false,30.);
    ensure("first player admitted",player.verdict == Verdict::ADMITTED);
    player.rebuild(2,102.);
    ensure("replacement returns to pending",player.verdict == Verdict::PENDING);
    ensure("replacement Playing gain is zero",!player.audible(Transport::PLAYING));
    ensure("paused replacement gain is zero",!player.audible(Transport::PAUSED));
    ensure("replacement has no inherited seek capability",!player.seekable);
    LLPluginFileAdmission viewer;
    viewer.queued(102.,60.);
    viewer.opened(102.);
    ensure_equals("paused replacement cannot time out",viewer.verdict(0,1,200.),0);
    // Actual decoding on the opened player starts long after the rebuild.
    player.resumed(200.);
    viewer.resumed(200.,1,2,0);
    player.observe(2,200.1,false,false,0.);
    ensure("late resume remains pending",player.verdict == Verdict::PENDING);
    ensure("late resume remains silent",!player.audible(Transport::PLAYING));
    ensure_equals("viewer does not spuriously stop on late resume",viewer.verdict(0,2,209.9),0);
    ensure("old player's video cannot admit replacement",!player.observe(1,201.,true,false,30.));
    player.observe(2,201.,true,false,0.);
    ensure("fresh track re-admits replacement",player.verdict == Verdict::ADMITTED);
    ensure("only verified replacement can gain audio",player.audible(Transport::PLAYING));
    ensure("verified paused player remains silent",!player.audible(Transport::PAUSED));
    player.rebuild(3,220.);
    player.observe(3,230.,false,false,0.);
    ensure("replacement still has bounded verification",player.verdict == Verdict::REJECTED);
    ensure_equals("viewer fresh window also expires",viewer.verdict(0,2,210.),-1);
}

template<> template<> void media_file_object::test<27>()
{
    set_test_name("raw BMFF metadata scan refuses hidden movies regardless of payload tiling");
    const auto ftyp = box("ftyp",Bytes{'q','t',' ',' ',0,0,0,0});
    const auto wrap = [&](const Bytes& b) { return join(ftyp,b); };
    ensure("free compressed movie plus four stray bytes refused",
        !container(wrap(box("free",join(box("cmov",{}),Bytes{1,2,3,4}))),"mov"));
    ensure("opaque foov with compressed reference movie refused",
        !container(wrap(box("foov",box("cmov",box("moov",box("rmra",{}))))),"mov"));
    ensure("free movie/reference plus one stray byte refused",
        !container(wrap(box("free",join(join(box("mvhd",{}),box("rmra",{})),Bytes{7}))),"mov"));
    ensure("uuid reference marker refused",!container(wrap(box("uuid",join(Bytes(16,0),Bytes{'r','m','r','a'}))),"mov"));
    for (const auto* root : {"free","skip","wide","uuid","udta","meta","zzzz","moov"})
        for (const auto* marker : {"cmov","rmra","rmda","rdrf","foov","dcom","cmvd"})
        {
            Bytes payload{1,2,3,0,0,0,8};
            payload.insert(payload.end(),marker,marker+4);
            payload.push_back(9);
            ensure("unaligned forbidden marker in every root refused",!container(wrap(box(root,payload)),"mov"));
        }
    for (const auto* marker : {"mvhd","moov","trak"})
    {
        Bytes payload{1,2,3,0,0,0,8}; payload.insert(payload.end(),marker,marker+4);
        ensure("unknown root cannot hide movie bytes",!container(wrap(box("zzzz",payload)),"mov"));
    }
    Bytes crossing(65526,'x'); // eight-byte root header puts marker at 65534
    crossing[crossing.size()-4] = 0; crossing[crossing.size()-3] = 0;
    crossing[crossing.size()-2] = 0; crossing[crossing.size()-1] = 8;
    crossing.insert(crossing.end(),{'r','m','r','a'});
    ensure("marker spanning scan chunks refused",!container(wrap(box("zzzz",crossing)),"mov"));
    ensure("ffmpeg empty free admitted",container(wrap(box("free",{})),"mp4"));
    ensure("iPhone wide admitted",container(wrap(box("wide",{})),"mov"));
    ensure("zero root padding admitted",container(wrap(Bytes(32,0)),"mov"));
    ensure("random textual free admitted",container(wrap(box("free",Bytes{'r','a','n','d','o','m',' ','t','e','x','t'})),"mov"));
    ensure("harmless short trailing fragment admitted",container(join(wrap(box("moov",{})),Bytes{1,2,3,4}),"mov"));
    ensure("short trailing marker refused",!container(join(wrap(box("moov",box("udta",Bytes(4,0)))),Bytes{'r','m','r','a'}),"mov"));
    ensure("malformed complete root header refused",!container(wrap(Bytes{0,0,0,4,'f','r','e','e'}),"mov"));
    ensure("malformed nested tail refused",!container(wrap(box("moov",Bytes{1,2,3,4})),"mov"));
    ensure("mdat marker bytes are opaque",container(wrap(box("mdat",Bytes{'c','m','o','v','f','o','o','v','r','m','r','a'})),"mov"));
    // Oversized metadata refuses before any payload read; mdat alone may skip it.
    for (bool media : {false,true})
    {
        const auto header = box(media ? "mdat" : "free",{});
        Bytes largeHeader = header; largeHeader[0] = 5;
        const auto size = be32(largeHeader.data());
        const auto suffix = box("free",Bytes(8,0)); // EOF probe stays outside the oversized payload
        size_t payloadReads = 0;
        bool admitted = true;
        try
        {
            checkContainer([&](uint64_t offset, void* dst, size_t count)
            {
                if (offset >= size && offset-size+count <= suffix.size())
                { std::memcpy(dst,suffix.data()+offset-size,count); return; }
                if (offset || count != 8) { ++payloadReads; throw std::runtime_error(NOT_VIDEO); }
                std::memcpy(dst,largeHeader.data(),count);
            },size+suffix.size(),"mov");
        }
        catch (const std::runtime_error&) { admitted = false; }
        ensure("only mdat may exceed metadata bound",admitted == media);
        ensure_equals("oversized payload is never read",payloadReads,size_t(0));
    }
}

template<> template<> void media_file_object::test<28>()
{
    set_test_name("resume preserves launch allowance until a player actually opens");
    LLPluginFileAdmission clock;
    clock.queued(100.,60.);
    clock.resumed(105.,1,2,0); // resume before opened notification
    ensure_equals("resume during launch cannot create verdict clock",clock.verdictDeadline,0.);
    ensure_equals("launch allowance survives ten seconds after resume",clock.verdict(0,2,125.),0);
    ensure_equals("hung resumed launch remains bounded",clock.verdict(0,2,160.),-1);
    clock.opened(150.);
    ensure_equals("opened after resume starts full verdict window",clock.verdictDeadline,160.);
    clock.queued(200.,60.);
    clock.opened(205.);
    clock.resumed(300.,1,2,0); // opened before late resume
    ensure_equals("opened paused player gets fresh resume window",clock.verdictDeadline,310.);
    ensure_equals("late resume does not stop",clock.verdict(0,2,309.9),0);
    clock.resumed(309.9,2,2,0);
    ensure_equals("duplicate playing cannot extend it",clock.verdict(0,2,310.),-1);
}

template<> template<> void media_file_object::test<29>()
{
    set_test_name("HTTP waits for Clear and three failed sweeps drop all hits");
    std::atomic<bool> clear{true}, cleanup{false}, degraded{false}, failed{false};
    unsigned failures = 0, sweeps = 0, waits = 0, drops = 0;
    unsigned entries = 2;
    awaitCacheClear(clear,[]() {},[&]()
    {
        ++waits;
        maintenanceWork([&]()
        {
            maintainCache(clear,cleanup,failures,degraded,[&](bool clearing)
            { ensure("retry is still a Clear",clearing); ++sweeps; throw std::runtime_error(FAILED); },
                [&]() { return entries != 0; },[&]() { ++drops; entries = 0; });
        },failed);
        if (waits < 3) ensure("failed Clear remains pending",clear.load() && cleanup.load());
    });
    ensure_equals("three failed sweeps bound Clear",sweeps,3u);
    ensure_equals("HTTP waits through each attempt",waits,3u);
    ensure_equals("drop happens once",drops,1u);
    ensure_equals("no stale cache hit remains",entries,0u);
    ensure("degraded completion releases HTTP",!clear && !cleanup && degraded && !failed);
    ensure_equals("waiting status",std::string(CLEARING),std::string("Clearing video cache..."));
    ensure_equals("degraded status",std::string(CLEAR_DEGRADED),std::string("Some cached videos couldn't be removed"));
    ensure_equals("real disk-full remains distinct",std::string(NO_SPACE),std::string("Not enough disk space"));
    clear = true;
    bool cancelled = false;
    try { awaitCacheClear(clear,[]() { throw std::runtime_error(FAILED); },[]() {}); }
    catch (const std::runtime_error&) { cancelled = true; }
    ensure("Stop can cancel waiting HTTP",cancelled);
}

template<> template<> void media_file_object::test<30>()
{
    set_test_name("fragment parser entry points land on scanned roots of the expected type");
    Bytes normal(std::begin(fragmentFixture),std::end(fragmentFixture));
    // Isolate offset regressions from the separate text-compatibility defect:
    // the old gate must admit this metadata before the malicious offsets.
    const std::string title = "formdata", harmless = "ordinary";
    const auto titleAt = std::search(normal.begin(),normal.end(),title.begin(),title.end());
    ensure("fixture carries title",titleAt != normal.end());
    std::copy(harmless.begin(),harmless.end(),titleAt);
    const auto put = [](Bytes& b, size_t at, uint64_t value, unsigned width)
    { for (unsigned i = 0; i < width; ++i) b[at+width-1-i] = static_cast<unsigned char>(value >> (8*i)); };
    const auto root = [&](const char* type)
    {
        for (size_t at = 0; at+8 <= normal.size(); at += static_cast<size_t>(be32(normal.data()+at)))
            if (std::memcmp(normal.data()+at+4,type,4) == 0) return at;
        throw std::runtime_error("missing fixture root");
    };
    const auto moof = root("moof"), mfra = root("mfra");
    ensure("actual ffmpeg frag_keyframe with mfra/mfro admitted",container(normal,"mp4"));
    Bytes noIndex(normal.begin(),normal.begin()+mfra);
    ensure("actual ffmpeg fragmentation without mfra admitted",container(noIndex,"mp4"));
    // Keep the real ffmpeg moov/mvex and first moof. The sidx correctly points
    // at that moof; only the EOF mfro redirects parsing into opaque mdat.
    Bytes prefix(normal.begin(),normal.begin()+moof);
    Bytes fragment(normal.begin()+moof,normal.begin()+moof+be32(normal.data()+moof));
    Bytes sid(36,0); put(sid,4,1,4); put(sid,8,1000,4);
    put(sid,22,1,2); put(sid,24,fragment.size(),4); put(sid,28,1000,4);
    const auto sidx = box("sidx",sid);
    Bytes hidden = box("moov",box("cmov",{}));
    const auto hiddenOffset = prefix.size()+sidx.size()+fragment.size()+8;
    Bytes trailer(8,0); put(trailer,4,hidden.size()+16,4);
    Bytes bad = join(join(join(prefix,sidx),fragment),box("mdat",join(hidden,box("mfro",trailer))));
    ensure("valid mvex/sidx cannot authorize mfro into mdat hidden movie",!container(bad,"mp4"));
    // Exact type, exact boundary, and nonwrapping subtraction are mandatory.
    for (uint64_t destination : {uint64_t(hiddenOffset),uint64_t(hiddenOffset+1),uint64_t(0)})
    {
        auto changed = normal; put(changed,changed.size()-4,changed.size()-destination,4);
        ensure("mfro wrong boundary/type refused",!container(changed,"mp4"));
    }
    auto underflow = normal; put(underflow,underflow.size()-4,UINT32_MAX,4);
    ensure("mfro underflow refused",!container(underflow,"mp4"));
    auto indexed = join(join(prefix,sidx),fragment);
    indexed = join(indexed,box("mdat",Bytes(16,0)));
    ensure("sidx to scanned moof admitted",container(indexed,"mp4"));
    for (uint64_t offset : {uint64_t(1),uint64_t(fragment.size()+8),uint64_t(UINT32_MAX)})
    {
        auto changed = indexed; put(changed,prefix.size()+8+16,offset,4);
        ensure("sidx interior/mdat/overflow destination refused",!container(changed,"mp4"));
    }
    auto hierarchy = indexed; put(hierarchy,prefix.size()+8+24,0x80000008,4);
    ensure("unsupported hierarchical sidx refused",!container(hierarchy,"mp4"));
    const size_t tfra = mfra+8, content = tfra+8;
    ensure("fixture starts mfra with tfra",std::memcmp(normal.data()+tfra+4,"tfra",4) == 0);
    const unsigned width = normal[content] ? 8 : 4;
    for (uint64_t destination : {uint64_t(moof+1),uint64_t(root("mdat")+8),uint64_t(root("moov"))})
    {
        auto changed = normal; put(changed,content+16+width,destination,width);
        ensure("tfra must identify scanned root moof",!container(changed,"mp4"));
    }
    auto eofMdat = noIndex; put(eofMdat,root("mdat"),0,4);
    ensure("fragmented zero-sized mdat with ambiguous next parser position refused",!container(eofMdat,"mp4"));
    Bytes largeMdat{0,0,0,1,'m','d','a','t',0,0,0,0,0,0,0,24};
    largeMdat.resize(24,0);
    ensure("fragmented largesize mdat cannot redirect sequential parsing",
        !container(join(join(prefix,fragment),largeMdat),"mp4"));
}

template<> template<> void media_file_object::test<34>()
{
    set_test_name("fragmented Apple chapter references cannot leave the box parser at sample data");
    const auto chapters = box("trak",box("tref",box("chap",Bytes{0,0,0,2})));
    const auto movie = box("moov",chapters);
    const auto fragmentedMovie = box("moov",join(chapters,box("mvex",{})));
    ensure("mvex with track chapter reference refused",!container(fragmentedMovie,"mp4"));
    ensure("later root moof with track chapter reference refused",
        !container(join(movie,box("moof",{})),"mp4"));
    ensure("earlier root moof with track chapter reference refused",
        !container(join(box("moof",{}),movie),"mp4"));
    ensure("non-fragmented track chapter reference admitted",container(movie,"mp4"));
    ensure("actual ffmpeg frag_keyframe without chapters admitted",
        container(Bytes(std::begin(fragmentFixture),std::end(fragmentFixture)),"mp4"));
    ensure("opaque chapter-like bytes are not track references",container(
        box("moov",join(box("mvex",{}),box("uuid",chapters))),"mp4"));
    ensure("other track references remain admitted",container(box("moov",join(
        box("mvex",{}),box("trak",box("tref",box("hint",Bytes{0,0,0,2}))))),"mp4"));
}

template<> template<> void media_file_object::test<31>()
{
    set_test_name("metadata text FourCC substrings need a plausible preceding size");
    ensure("actual ffmpeg title formdata admitted",
        container(Bytes(std::begin(fragmentFixture),std::end(fragmentFixture)),"mp4"));
    const std::string words = "title formdata trakt moovie cmovie";
    const Bytes text(words.begin(),words.end());
    for (const auto* type : {"XMP_","uuid","zzzz","free"})
        ensure("ordinary opaque text admitted",container(box(type,text),"mp4"));
    ensure("udta title text admitted",container(box("moov",box("udta",box("name",text))),"mov"));
    ensure("ISO meta text admitted",container(box("meta",join(Bytes(4,0),box("XMP_",text))),"mp4"));
    ensure("QuickTime meta text admitted",container(box("meta",join(box("hdlr",{}),box("XMP_",text))),"mov"));
    for (uint32_t size : {0u,1u,8u})
    {
        Bytes header{0,0,0,static_cast<unsigned char>(size),'r','m','r','a'};
        ensure("real or extended reference box still refused",!container(box("uuid",header),"mp4"));
    }
    ensure("real rmra size eight refused",!container(box("moov",box("rmra",{})),"mov"));
    // Size 8 fits the ROOT even though it extends beyond nested free padding.
    ensure("padding uses enclosing root bound",!container(box("moov",join(
        box("free",Bytes{0,0,0,16,'m','v','h','d'}),box("udta",{}))),"mov"));
}

template<> template<> void media_file_object::test<32>()
{
    set_test_name("Browse failure overlay expires or yields to the slot's own status");
    BrowseOverlay overlay;
    std::string status = "checking file", own = "Playing";
    overlay.apply(status,true,own,1000);
    ensure_equals("pending shown",own,status);
    overlay.restore(own);
    ensure_equals("committed slot reason retained",own,std::string("Playing"));
    status = NOT_VIDEO; overlay.apply(status,false,own,2000);
    ensure_equals("failure shown once",own,std::string(NOT_VIDEO));
    overlay.restore(own); overlay.apply(status,false,own,6999);
    ensure_equals("failure still visible before five seconds",own,std::string(NOT_VIDEO));
    overlay.restore(own); overlay.apply(status,false,own,7000);
    ensure("failure expires",status.empty());
    ensure_equals("underlying reason restored",own,std::string("Playing"));
    status = MISSING; overlay.apply(status,false,own,8000);
    overlay.restore(own); own = "Paused"; overlay.apply(status,false,own,8001);
    ensure("slot status change clears failure immediately",status.empty());
    ensure_equals("new slot reason survives",own,std::string("Paused"));
    overlay.apply(status,false,own,20000);
    ensure_equals("failure never reappears",own,std::string("Paused"));
    status = NOT_VIDEO; overlay.apply(status,false,own,21000,1);
    overlay.restore(own); overlay.apply(status,false,own,21001,2);
    ensure("health/output/activity change clears overlay even with the same reason",status.empty());
}

template<> template<> void media_file_object::test<33>()
{
    set_test_name("degraded Clear retains bytes and retries orphan unlink every sixty seconds");
    struct Record { uint64_t size = 0; unsigned leases = 0; };
    std::map<std::string,std::shared_ptr<Record>> entries;
    std::vector<std::shared_ptr<Record>> evicted;
    entries["locked"] = std::make_shared<Record>(Record{70,1});
    CacheOrphans orphans; orphans.files["orphan"] = 20;
    bool clear = true, cleanup = false, degraded = false;
    unsigned failures = 0;
    for (unsigned i = 0; i < 3; ++i)
        maintenanceItem([&]()
        {
            maintainCache(clear,cleanup,failures,degraded,[](bool) { throw std::runtime_error(FAILED); },
                [&]() { return !entries.empty(); },[&]() { dropCacheHits(entries,evicted); });
        },[]() {});
    ensure("Clear degraded and hit gate released",degraded && !clear && entries.empty());
    ensure_equals("lease and file accounting retained",evicted.size(),size_t(1));
    ensure_equals("known bytes survive dropped hits",cacheUsage(entries,evicted,orphans),uint64_t(90));
    ensure("reservation cannot exceed cap",!cacheFits(cacheUsage(entries,evicted,orphans),11,100));
    ensure("remaining capacity usable",cacheFits(cacheUsage(entries,evicted,orphans),10,100));
    unsigned unlinks = 0;
    orphans.retry(1000,[&](const std::string&) { ++unlinks; return false; });
    orphans.retry(60999,[&](const std::string&) { ++unlinks; return true; });
    ensure_equals("failed unlink not retried early",unlinks,1u);
    ensure_equals("failed unlink remains charged",orphans.bytes(),uint64_t(20));
    orphans.retry(61000,[&](const std::string&) { ++unlinks; return true; });
    ensure_equals("next maintenance retries unlink",unlinks,2u);
    ensure_equals("only successful unlink releases bytes",orphans.bytes(),uint64_t(0));
    orphans.unknown = true;
    ensure("failed inventory refuses reservations",!cacheFits(cacheUsage(entries,evicted,orphans),1,100));
    orphans.unknown = false;
    for (unsigned i = 0; i < 300; ++i) orphans.files[std::to_string(i)] = 1;
    unsigned attempts = 0;
    std::set<std::string> visited;
    for (unsigned i = 0; i < 3; ++i)
    {
        unsigned pass = 0;
        orphans.retry(121000+60000*i,[&](const std::string& name)
        { ++attempts; ++pass; visited.insert(name); return false; });
        ensure("maintenance unlink work bounded",pass <= 128);
    }
    ensure_equals("persistent first files cannot starve later orphans",visited.size(),size_t(300));
    ensure_equals("three bounded passes",attempts,384u);
    CacheOrphans partial;
    partial.abandon("partial-write",0,90);
    ensure_equals("failed partial write retains the entire reservation",partial.bytes(),uint64_t(90));
    ensure("counter lag cannot free capacity before unlink",!cacheFits(partial.bytes(),11,100));
}

}

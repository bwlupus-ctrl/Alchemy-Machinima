/** Pure private-file gates; no filesystem, network, plugin or viewer fixture. */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alvcammediafilepolicy.h"
#include "../alvcammediafilecontainer.h"
#include "../alvcammediafilelifecycle.h"
#include "../alvcammediafilecache.h"
#include "../alkitqol.h"
#include <chrono>
#include "../../llplugin/llpluginfileclose.h"
#include "../../llplugin/llpluginfileadmission.h"
#include <array>
#include <cstring>
#include <set>
#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <sys/stat.h>
#if LL_WINDOWS
#include <io.h>
#endif
#include <iostream>
#include <atomic>
#include <deque>
#include <thread>
#include "alvcamfragmentfixture.h"

namespace tut
{
struct media_file_data {};
typedef test_group<media_file_data, 65> media_file_group;
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
    awaitCacheClear(clear, CACHE_CLEAR_WAIT_MS, []() { return int64_t(0); }, []() {},[&]()
    {
        ++waits;
        maintenanceWork([&]()
        {
            maintainCache(clear,cleanup,failures,degraded,[&](bool clearing)
            { ensure("retry is still a Clear",clearing); ++sweeps; throw std::runtime_error(FAILED); },
                [&]() { return entries != 0; },[&]() { ++drops; entries = 0; }, []() { return true; });
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
    try { awaitCacheClear(clear, CACHE_CLEAR_WAIT_MS, []() { return int64_t(0); }, []() { throw std::runtime_error(FAILED); },[]() {}); }
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
    set_test_name("Browse refusal persists until an explicit slot action clears it");
    BrowseOverlay overlay;
    std::string status = "checking file", own = "Playing";
    overlay.apply(status,true,own,1000);
    ensure_equals("pending shown",own,status);
    overlay.restore(own);
    ensure_equals("committed slot reason retained",own,std::string("Playing"));
    status = NOT_VIDEO; overlay.apply(status,false,own,2000);
    ensure_equals("failure shown",own,std::string(NOT_VIDEO));
    overlay.restore(own); overlay.apply(status,false,own,7000);
    ensure_equals("five seconds cannot erase refusal",own,std::string(NOT_VIDEO));
    overlay.restore(own); own = "Paused"; overlay.apply(status,false,own,20000,2);
    ensure_equals("live status and health cannot erase refusal",own,std::string(NOT_VIDEO));
    overlay.restore(own);
    ensure_equals("latest underlying status restored",own,std::string("Paused"));
    status.clear(); overlay.apply(status,false,own,30000);
    ensure_equals("explicit action clears refusal",own,std::string("Paused"));
    overlay.apply(status,false,own,40000);
    ensure_equals("cleared refusal never reappears",own,std::string("Paused"));
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
                [&]() { return !entries.empty(); },[&]() { dropCacheHits(entries,evicted); }, []() { return true; });
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

// Opt-in real-file harness: no viewer, plugin, cache or network initialization.
// A rejected probe is a reported verdict, not a failure of this test runner.
template<> template<> void media_file_object::test<35>()
{
    set_test_name("VCAM_MEDIA_PROBE_FILE production container probe (opt-in)");
    std::string value;
#if LL_WINDOWS
    if (const wchar_t* native = _wgetenv(L"VCAM_MEDIA_PROBE_FILE"))
    {
        const auto utf8 = std::filesystem::path(native).u8string();
        value.assign(utf8.begin(), utf8.end());
    }
#else
    if (const char* native = std::getenv("VCAM_MEDIA_PROBE_FILE")) value = native;
#endif
    if (value.empty())
    {
        std::cout << "VCAM_MEDIA_PROBE_FILE: SKIPPED (not set)\n";
        return;
    }
    uint64_t size = 0;
    bool have_size = false;
    std::string ext = diagnosticExtension(value);
    try
    {
        Descriptor source; std::string reason;
        if (!classify(value, source, reason, LL_WINDOWS) || source.location != Location::LOCAL_FILE)
        {
            std::cout << "VCAM_MEDIA_PROBE_FILE: REJECTED rule=local-file syntax reason=" << (reason.empty() ? UNSUPPORTED : reason)
                      << " size=unknown ext=" << ext << '\n';
            return;
        }
        const auto path = std::filesystem::path(std::u8string(source.value.begin(), source.value.end()));
        FILE* opened = nullptr;
#if LL_WINDOWS
        if (_wfopen_s(&opened, path.c_str(), L"rb") != 0) throw std::runtime_error(MISSING);
#else
        opened = std::fopen(path.c_str(), "rb");
#endif
        if (!opened) throw std::runtime_error(MISSING);
        const std::unique_ptr<FILE, decltype(&std::fclose)> file(opened, std::fclose);
        const auto snapshot = [&]()
        {
#if LL_WINDOWS
            struct _stat64 info{};
            if (_fstat64(_fileno(file.get()), &info)) throw PolicyRefusal("probe descriptor stat failed");
#else
            struct stat info{};
            if (fstat(fileno(file.get()), &info)) throw PolicyRefusal("probe descriptor stat failed");
#endif
            if ((info.st_mode & S_IFMT) != S_IFREG) throw PolicyRefusal("probe requires a regular file");
            return FileIdentity{static_cast<uint64_t>(info.st_dev), static_cast<uint64_t>(info.st_ino),
                static_cast<uint64_t>(info.st_size), info.st_mtime, 0};
        };
        size = snapshot().size; have_size = true;
        const auto read = [&](uint64_t offset, void* dst, size_t count)
        {
#if LL_WINDOWS
            if (_fseeki64(file.get(), static_cast<int64_t>(offset), SEEK_SET)) throw PolicyRefusal("probe seek failed");
#else
            if (fseeko(file.get(), static_cast<off_t>(offset), SEEK_SET)) throw PolicyRefusal("probe seek failed");
#endif
            if (std::fread(dst, 1, count, file.get()) != count) throw PolicyRefusal("probe short read");
        };
        // validateFile calls the real checkContainer; no test reimplementation.
        validateFile(read, snapshot, extension(source.value));
        std::cout << "VCAM_MEDIA_PROBE_FILE: ADMITTED rule=validateFile/checkContainer reason=passed"
                  << " size=" << size << " ext=" << ext << '\n';
    }
    catch (const std::exception& error)
    {
        std::cout << "VCAM_MEDIA_PROBE_FILE: REJECTED rule=" << diagnosticReason(error)
                  << " reason=" << (dynamic_cast<const PolicyRefusal*>(&error) ? NOT_VIDEO : diagnosticReason(error))
                  << " size=" << (have_size ? std::to_string(size) : "unknown") << " ext=" << ext << '\n';
    }
}

template<> template<> void media_file_object::test<36>()
{
    set_test_name("Diagnostic rules preserve public refusals and never echo input");
    const PolicyRefusal refusal("dref entry is not a self-contained url/alias");
    ensure_equals("public status unchanged",std::string(refusal.what()),std::string(NOT_VIDEO));
    ensure_equals("precise rule retained",diagnosticReason(refusal),refusal.rule);
    ensure_equals("exception secrets omitted",diagnosticReason(std::runtime_error("https://user:secret@host/a.mp4?key=secret")),std::string("I/O or transfer exception"));
    ensure_equals("native hash is not a URL fragment",diagnosticExtension("D:/Footage/a#b.MOV"),std::string("mov"));
    ensure_equals("file URI decoded for extension",diagnosticExtension("file:///D:/Footage/a%2Emp4"),std::string("mp4"));
    ensure_equals("only safe extension retained",diagnosticExtension("https://user:secret@host/a.mp4?key=secret"),std::string("mp4"));
    ensure_equals("unsupported extension never echoes input",diagnosticExtension("https://host/a.secret?key=secret"),std::string("unsupported"));
}

template<> template<> void media_file_object::test<37>()
{
    set_test_name("native repeat option and admission latch policy (no VLC seam claim)");
    ensure_equals("loop option", std::string(inputRepeatOption(true)), std::string(":input-repeat=65535"));
    ensure_equals("Gate Play-to-end exposes EOF", std::string(inputRepeatOption(false)), std::string(":input-repeat=0"));
    for (double duration : {0., 30.})
    {
        Admission player;
        player.open(42, 100.);
        ensure("repeating pending input stays muted", !player.audible(Transport::PLAYING));
        ensure("pending input cannot publish", !player.retainsOutput());
        player.observe(42, 110., false, false, duration);
        ensure("never-admitted repeat expires", player.verdict == Verdict::REJECTED);
        player.open(43, 111.);
        player.observe(43, 112., true, false, duration);
        ensure("known and unknown duration admit video", player.audible(Transport::PLAYING));
        ensure_equals("only unknown duration can recover by reopen", unknownLoopRecovery(player, true, Transport::PLAYING, duration), duration == 0.);
        ensure("Loop OFF exposes EOF instead of recovery", !unknownLoopRecovery(player, false, Transport::PLAYING, duration));
        ensure("paused cannot auto-recover", !unknownLoopRecovery(player, true, Transport::PAUSED, duration));
        player.rebuild(44, 113., true);
        ensure("explicit loop edit re-admits muted", !player.audible(Transport::PLAYING));
        ensure("explicit edit retains accepted old output only", player.retainsOutput());
        player.observe(44, 123., false, false, duration);
        ensure("edit deadline revokes hold", !player.retainsOutput());
    }
}

template<> template<> void media_file_object::test<39>()
{
    set_test_name("wrap admission latch preserves marker-time state across same-tick admission");
    using W = WrapPolicy;
    for (bool observeBeforeMarker : {false, true})
    {
        Admission player; player.open(7, 0.);
        unsigned state = 0; // tick already exchanged its event flags
        if (observeBeforeMarker) player.observe(7, .1, true, false, 30.);
        state = W::marker(state); // callback while admission is still unpublished
        if (!observeBeforeMarker) player.observe(7, .1, true, false, 30.);
        state = W::admit(state);
        if (W::rejects(state)) player.observe(7, .1, true, true, 30.);
        ensure("pending marker always rejects even after a local video verdict", player.verdict == Verdict::REJECTED);
        ensure("pending marker never enables publication", !(state & W::Admitted));
        ensure("rejected input is muted", !player.audible(Transport::PLAYING));
        ensure_equals("repeated pending markers cannot clear refusal", W::marker(state), state);
    }
    unsigned state = W::admit(0);
    state = W::marker(state); // callback AFTER atomic admission
    ensure("normal admitted wrap does not reject", !W::rejects(state) && (state & W::Admitted));
    // Force the callback RMW between the tick's load and its admission CAS.
    std::atomic<unsigned> latch{0};
    unsigned expected = latch.load();
    const unsigned proposal = W::admit(expected);
    latch.fetch_or(W::Marker);
    ensure("stale admission CAS fails", !latch.compare_exchange_strong(expected, proposal));
    ensure("retry uses sticky marker", latch.compare_exchange_strong(expected, W::admit(expected)));
    ensure("CAS retry cannot erase pending marker", W::rejects(latch.load()));
}

template<> template<> void media_file_object::test<40>()
{
    set_test_name("pending time regression fallback needs a stable first step, independent of debug marker");
    WrapPolicy startup;
    for (double time : {-1., 8., 0., 0.})
        ensure("non-monotonic startup is not a wrap", !startup.timeRegression(time, 30., true));
    ensure("first forward step establishes stability", !startup.timeRegression(.1, 30., true));
    ensure("small jitter is harmless", !startup.timeRegression(.09, 30., true));
    for (double duration : {0., 30.})
    {
        WrapPolicy fallback;
        ensure("initial sample", !fallback.timeRegression(2., duration, true));
        ensure("stable forward step", !fallback.timeRegression(3., duration, true));
        ensure("strict 1s threshold", !fallback.timeRegression(2., duration, true));
        ensure("more than 1s rejects even with unknown duration", fallback.timeRegression(.5, duration, true));
    }
    WrapPolicy shortClip;
    shortClip.timeRegression(.1, 1., true); shortClip.timeRegression(.9, 1., true);
    ensure("more than 50 percent catches a short wrap", shortClip.timeRegression(.2, 1., true));
    WrapPolicy admitted;
    admitted.timeRegression(2., 30., false); admitted.timeRegression(3., 30., false);
    ensure("fallback cannot revoke an admitted input", !admitted.timeRegression(0., 30., false));
    WrapPolicy invalid;
    invalid.timeRegression(2., 30., true); invalid.timeRegression(3., 30., true);
    ensure("unavailable clock resets stable step", !invalid.timeRegression(-1., 30., true));
    ensure("fresh clock needs a new step", !invalid.timeRegression(0., 30., true));
}

template<> template<> void media_file_object::test<41>()
{
    set_test_name("Loop edit resume skips non-seekable input and releases failed seeks within 500ms");
    using Action = ResumeWait::Action;
    ResumeWait resume;
    resume.open(90., false);
    ensure("duration-less/non-seekable never requests seek", resume.step(100., true, true, 0.) == Action::READY);
    resume.open(90., true);
    ensure("wait for playing", resume.step(100., false, false, 0.) == Action::WAIT);
    ensure("seek only once", resume.step(100.1, true, false, 0.) == Action::SEEK);
    ensure("reported seek refusal admits immediately", resume.step(100.11, true, true, 0.) == Action::READY);
    resume.open(90., true);
    ensure("request resume", resume.step(200., true, false, 0.) == Action::SEEK);
    ensure("bounded wait retains mute until timeout", resume.step(200.499, true, false, .4) == Action::WAIT);
    ensure("silent demux seek refusal releases at 500ms", resume.step(200.5, true, false, .5) == Action::READY);
    Admission player; player.open(1, 200.);
    player.observe(1, 200.5, true, false, 0.);
    ensure("failed resume does not cause 10s not-video refusal", player.verdict == Verdict::ADMITTED);
    resume.open(90., true);
    resume.step(300., true, false, 0.);
    ensure("successful seek releases without waiting for timeout", resume.step(300.1, true, false, 90.) == Action::READY);
    resume.open(90., true);
    ensure("slow open beyond 500ms keeps target", resume.step(402., false, false, 0.) == Action::WAIT);
    ensure("slow open still seeks on first Playing", resume.step(403., true, false, 0.) == Action::SEEK);
    ensure("missing Playing after seek cannot extend armed deadline", resume.step(403.5, false, false, 0.) == Action::READY);

    resume.open(90., true);
    // A paused replacement is not ticked until Play, even after a long delay.
    resume.resumed();
    ensure("extended pause then Play still seeks", resume.step(900., true, false, 0.) == Action::SEEK);
    ensure("extended pause resumes at target", resume.step(900.1, true, false, 90.) == Action::READY);
    resume.open(90., true);
    resume.step(1000., true, false, 0.);
    resume.resumed(); // Pause while the first seek is pending, then Play.
    ensure("Play re-arms an already armed pending replacement", resume.step(1100., true, false, 0.) == Action::SEEK);
    ensure("re-armed seek still bounded", resume.step(1100.5, true, false, 0.) == Action::READY);

    resume.open(90., true);
    ensure_equals("pending re-toggle overrides unavailable replacement sample", resume.loopTarget(0., false), 90.);
    resume.open(resume.loopTarget(0., false), true);
    ensure("re-toggle during open still seeks", resume.step(1200., true, false, 0.) == Action::SEEK);
    resume.open(resume.loopTarget(.1, true), true);
    ensure_equals("re-toggle during seek keeps original target", resume.position, 90.);
    ensure("second replacement seeks original target", resume.step(1300., true, false, 0.) == Action::SEEK);
    ensure("second replacement reaches original target", resume.step(1300.1, true, false, 90.) == Action::READY);
    ensure_equals("completed resume uses current sample", resume.loopTarget(20., true), 20.);
    ensure_equals("no pending target preserves non-seekable skip", resume.loopTarget(20., false), 0.);
    player.open(2, 500.);
    player.observe(2, 510., false, false, 0.);
    ensure("ordinary never-video deadline remains strict", player.verdict == Verdict::REJECTED);
}

template<> template<> void media_file_object::test<42>()
{
    set_test_name("session-only quit schedules detached cleanup without inline deletion");
    unsigned deleted = 0;
    std::function<void()> worker;
    ensure("owner schedules", scheduleSessionCleanup(true, false, [&]()
    { worker = [&]() { ++deleted; }; }));
    ensure_equals("quit only posts work", deleted, 0u);
    worker();
    ensure_equals("worker performs deletion", deleted, 1u);
}

template<> template<> void media_file_object::test<43>()
{
    set_test_name("session-only startup clears indexed downloads and interrupted leftovers");
    ensure("off sweeps without Preferences purge", clearCacheAtStartup(true, false, false));
    ensure("off plus purge clears", clearCacheAtStartup(true, false, true));
}

template<> template<> void media_file_object::test<44>()
{
    set_test_name("keep on preserves downloads across quit and startup");
    ensure("startup preserves", !clearCacheAtStartup(true, true, false));
    ensure("quit preserves", !scheduleSessionCleanup(true, true, []()
    { throw std::runtime_error("keep must not schedule cleanup"); }));
}

template<> template<> void media_file_object::test<45>()
{
    set_test_name("Preferences next-start purge overrides keep on");
    for (bool keep : {false, true})
        ensure("lock owner clears regardless of keep", clearCacheAtStartup(true, keep, true));
}

template<> template<> void media_file_object::test<46>()
{
    set_test_name("non-owner cannot schedule quit or startup cache mutation");
    for (bool keep : {false, true})
    {
        for (bool purge : {false, true})
            ensure("startup untouched", !clearCacheAtStartup(false, keep, purge));
        ensure("quit untouched", !scheduleSessionCleanup(false, keep, []()
        { throw std::runtime_error("non-owner must not schedule cleanup"); }));
    }
}

template<> template<> void media_file_object::test<47>()
{
    set_test_name("turning keep off defers playing files until quit and lease close");
    unsigned leases = 2, scheduled = 0;
    bool keep = true;
    keep = false; // preference edit alone never calls the cleanup scheduler
    ensure_equals("editing preference schedules nothing", scheduled, 0u);
    ensure("quit schedules with live leases", scheduleSessionCleanup(true, keep, [&]() { ++scheduled; }));
    ensure("playing file deferred", deferCacheDeletion(leases));
    ensure("other player holds it", deferCacheDeletion(--leases));
    ensure("closed file may be swept", !deferCacheDeletion(--leases));
    ensure_equals("one quit cleanup", scheduled, 1u);
}

template<> template<> void media_file_object::test<38>()
{
    set_test_name("disabled cue status clears on enabling either master/media slots");
    for (const char* disabled : {"Turn on Virtual Cam to play video", "Media slots disabled"})
        for (bool empty : {false, true})
        {
            std::string reason = disabled;
            ensure("off status preserved", !clearDisabledStatus(reason, false, empty));
            ensure("enable replaces disabled reason", clearDisabledStatus(reason, true, empty));
            ensure_equals("current cue status", reason, std::string(empty ? "Enter a direct video URL" : "Set. Press Play to start it."));
        }
    for (bool master : {false, true})
        for (bool slots : {false, true})
        {
            std::string playStatus = "Turn on Virtual Cam to play video";
            ensure("unchanged policy preserves status", !clearPlayStatusOnPolicyChange(playStatus, master, slots, master, slots));
            ensure("master edit clears Play status even with slots off", clearPlayStatusOnPolicyChange(playStatus, !master, slots, master, slots) && playStatus.empty());
            playStatus = "Media slots disabled";
            ensure("slots edit clears Play status even with master off", clearPlayStatusOnPolicyChange(playStatus, master, !slots, master, slots) && playStatus.empty());
        }
    std::string refused = NOT_VIDEO;
    ensure("unrelated refusal preserved", !clearDisabledStatus(refused, true, false));
}


// Private, disposable direct children only; never touches the viewer cache.
struct CacheFixFixture
{
    std::filesystem::path root;
    CacheFixFixture()
    {
        root = std::filesystem::temp_directory_path() /
            ("vcam-cache-fix-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(root)) throw std::runtime_error("test directory collision");
    }
    ~CacheFixFixture()
    {
        std::error_code error;
        for (const auto* name : {"index", "index.tmp", "orphan.mp4"}) std::filesystem::remove(root / name, error);
        std::filesystem::remove(root, error);
    }
    void write(const char* name, const std::string& bytes)
    {
        std::ofstream out(root / name, std::ios::binary | std::ios::trunc);
        out << bytes;
        out.close();
        if (!out) throw std::runtime_error("fixture write failed");
    }
    std::string record() const { return std::string(64, 'a') + " 1 mp4 video/mp4 12 3\n"; }
    void sweepAndReplace(const CacheIndexSnapshot& snapshot)
    {
        // Recovery discards corrupt metadata, so unindexed bytes become orphans.
        if (snapshot.entries.empty()) std::filesystem::remove(root / "orphan.mp4");
        removeCacheIndexTemporary(root);
        write("index.tmp", snapshot.entries.empty() ? "" : record());
        replaceCacheIndex(root);
    }
};

template<> template<> void media_file_object::test<48>()
{
    set_test_name("Windows index reader sharing regression and closed-reader sweep/replace");
    CacheFixFixture cache;
    cache.write("index", cache.record());
    cache.write("index.tmp", "");
#if LL_WINDOWS
    {
        std::ifstream reader(cache.root / "index", std::ios::binary);
        ensure("index reader opened", reader.good());
        bool sharing = false;
        try { replaceCacheIndex(cache.root); }
        catch (const std::system_error& error)
        {
            std::cout << "Held MSVC index reader replacement: Win32 error=" << error.code().value() << '\n';
            sharing = error.code().value() == ERROR_SHARING_VIOLATION || error.code().value() == ERROR_ACCESS_DENIED;
        }
        ensure("negative control: MSVC reader denies FILE_SHARE_DELETE", sharing);
    }
#endif
    const auto snapshot = readCacheIndex(cache.root / "index");
    ensure_equals("valid metadata read", snapshot.entries.size(), size_t(1));
    ensure("not recovery", !snapshot.recovered);
    cache.sweepAndReplace(snapshot); // Production reader must already be closed.
    ensure_equals("replacement reopens successfully", readCacheIndex(cache.root / "index").entries.size(), size_t(1));
    ensure("temporary consumed", !std::filesystem::exists(cache.root / "index.tmp"));
}

template<> template<> void media_file_object::test<49>()
{
    set_test_name("Corrupt/partial metadata discards the entire index; empty and missing indexes recover");
    CacheFixFixture cache;
    for (const auto& bytes : {cache.record() + "broken", cache.record() + std::string(64, 'b') + " 2 mp4",
        cache.record() + cache.record(), std::string(64, 'a') + " -1 mp4 video/mp4 12 3\n",
        std::string(64, 'a') + " 2 mp4 video/mp4 0 3\n"})
    {
        cache.write("index", bytes);
        cache.write("orphan.mp4", "unindexed bytes");
        cache.write("index.tmp", "stale interrupted publication");
        const auto snapshot = readCacheIndex(cache.root / "index");
        ensure("corrupt metadata reported", snapshot.recovered);
        ensure("valid prefix never published", snapshot.entries.empty());
        cache.sweepAndReplace(snapshot);
        ensure("orphan swept", !std::filesystem::exists(cache.root / "orphan.mp4"));
        const auto rebuilt = readCacheIndex(cache.root / "index");
        ensure("empty rebuilt index valid", !rebuilt.recovered && rebuilt.entries.empty());
    }
    cache.write("index.tmp", "stale");
    removeCacheIndexTemporary(cache.root);
    ensure("stale temporary explicitly removed", !std::filesystem::exists(cache.root / "index.tmp"));
    removeCacheIndexTemporary(cache.root); // Missing temporary is harmless.
    std::filesystem::remove(cache.root / "index");
    const auto missing = readCacheIndex(cache.root / "index");
    ensure("missing index recoverable", missing.recovered && missing.entries.empty());
    cache.sweepAndReplace(missing);
    ensure("missing index rebuilt", !readCacheIndex(cache.root / "index").recovered);
}

template<> template<> void media_file_object::test<50>()
{
    set_test_name("Ownership failure retries once per Play and coalesces pending attempts without a session latch");
    std::atomic<int> state{0};
    ensure("startup checking does not retry", !requestCacheOwnerRetry(state));
    state = 1;
    ensure("owner never reinitializes", !requestCacheOwnerRetry(state));
    CacheFixFixture cache;
    cache.write("index", "");
    cache.write("index.tmp", "");
    state = -1;
#if LL_WINDOWS
    {
        std::ifstream blocked(cache.root / "index", std::ios::binary);
        try { replaceCacheIndex(cache.root); state = 1; }
        catch (const std::system_error&) { state = -1; }
        ensure_equals("initial replacement fails", state.load(), -1);
    }
#endif
    ensure("next Play schedules recovery", requestCacheOwnerRetry(state));
    ensure_equals("admitted retry publishes checking state", state.load(), 0);
    ensure("concurrent Play shares in-progress attempt", !requestCacheOwnerRetry(state));
    cache.sweepAndReplace(readCacheIndex(cache.root / "index"));
    state = 1;
    ensure("retry replaces after held reader closes", !readCacheIndex(cache.root / "index").recovered);
    ensure("successful retry stops", !requestCacheOwnerRetry(state));
    for (unsigned play = 0; play < 5; ++play)
    {
        state = -1; // A later transient failure still permits the next explicit Play.
        ensure("later Play never session-latched", requestCacheOwnerRetry(state));
        ensure("only one retry admitted per failed attempt", !requestCacheOwnerRetry(state));
        ensure_equals("pending retry bounds admission", state.load(), 0);
    }
}

template<> template<> void media_file_object::test<51>()
{
    set_test_name("Tile and Source Status share current failures ahead of stale editor prompts");
    for (const auto& runtime : {std::string(FAILED), std::string(FAILED) + " (HTTP 403)",
        cacheFailureStatus(CacheFailure::IO), cacheFailureStatus(CacheFailure::ACCESS), std::string(LOCKED),
        std::string(NOT_VIDEO), std::string(NO_SPACE), std::string("Unsupported player: VLC"),
        std::string("Copy failed: media sampler unavailable; retrying"), std::string("Source missing or unbound")})
        for (const char* stale : {"Enter a direct video URL", "Set. Press Play to start it.", UNSUPPORTED, ""})
        {
            ensure("tile recognizes failure", mediaRuntimeFailure(runtime));
            const auto tile = mediaSourceStatus(runtime, "");
            ensure_equals("Source Status matches tile", mediaSourceStatus(runtime, stale), tile);
            ensure_equals("exact runtime failure preserved", tile, runtime);
        }
    ensure_equals("active runtime permits editor error", mediaSourceStatus("Playing", UNSUPPORTED), std::string(UNSUPPORTED));
    ensure_equals("progress is not a failure", mediaSourceStatus("Downloading 45%", "draft"), std::string("draft"));
    ensure_equals("typed cache status", cacheFailureStatus(CacheFailure::IO), std::string("Video cache unavailable: cache I/O failed"));
}

template<> template<> void media_file_object::test<52>()
{
    set_test_name("All text rows paste OS text through validation; numeric/combo retain value clipboard");
    std::string os = "https://example.test/new.mp4", draft = "old", cue = "https://example.test/playing.mp4", error;
    unsigned reads = 0, commits = 0, replacements = 0;
    auto read = [&]() -> LLSD { ++reads; return os.empty() ? LLSD() : LLSD(os); };
    auto commit = [&](const LLSD& value)
    {
        ++commits;
        draft = value.asString(); // Normal editor path replaces the entire field.
        return commitVideoDraft(draft, error, true, [&](const Descriptor& descriptor)
        {
            ++replacements; // The real panel's source replacement/possible stop boundary.
            cue = descriptor.value;
            return true;
        });
    };
    ensure("OS text enabled without value clipboard", ALKitQol::pasteRowValue(true, LLSD(), read, commit));
    ensure_equals("whole field replaced", draft, os);
    ensure_equals("valid cue committed", cue, os);
    os = "not a video URL";
    ensure("invalid commit refused", !ALKitQol::pasteRowValue(true, LLSD("stale value"), read, commit));
    ensure_equals("invalid OS text remains draft", draft, os);
    ensure_equals("validation reason retained", error, std::string(UNSUPPORTED));
    ensure_equals("accepted cue survives invalid paste", cue, std::string("https://example.test/new.mp4"));
    ensure_equals("invalid paste never reaches replacement/stop boundary", replacements, 1u);
    os.clear();
    ensure("clipboard disappearing does not paste stale value", !ALKitQol::pasteRowValue(true, LLSD("stale"), read, commit));
    ensure_equals("missing OS text does not commit", commits, 2u);
    LLSD applied;
    ensure("numeric paste", ALKitQol::pasteRowValue(false, LLSD(7), read, [&](const LLSD& v) { applied = v; return true; }));
    ensure_equals("typed numeric preserved", applied.asInteger(), 7);
    ensure("combo value paste", ALKitQol::pasteRowValue(false, LLSD("combo"), read, [&](const LLSD& v) { applied = v; return true; }));
    ensure_equals("combo uses value clipboard", applied.asString(), std::string("combo"));
    ensure_equals("numeric/combo never read OS text", reads, 3u);
}

template<> template<> void media_file_object::test<53>()
{
    set_test_name("executed retire captures close deny-delete pins before final quit sweep");
    CacheFixFixture cache;
    cache.write("orphan.mp4", "playing video");
    std::deque<std::function<void()>> garbage;
    unsigned leases = 1;
#if LL_WINDOWS
    HANDLE raw = CreateFileW((cache.root / "orphan.mp4").c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ensure("validation deny-delete handle opens", raw != INVALID_HANDLE_VALUE);
    auto pin = std::shared_ptr<void>(raw, [](void* h) { CloseHandle(h); });
#else
    auto pin = std::make_shared<int>(0);
#endif
    std::weak_ptr<void> held = pin;
    // Outer result destruction queues the real cache-lease retirement, just as
    // mailbox destruction and DeferredClose do in production.
    auto retention = std::shared_ptr<void>(new int(0), [&, pin](void* p)
    {
        delete static_cast<int*>(p);
        garbage.push_back([&, pin]() { --leases; });
    });
    garbage.push_back([retention]() {});
    retention.reset(); pin.reset();
    std::deque<std::function<void()>> batch;
    batch.swap(garbage);
    executeRetirements(batch, [](auto& fn) { fn(); });
    ensure("recursive release requires another deletion pass",
        !finishShutdownMaintenance(false, true, !garbage.empty(), false));
    batch.swap(garbage);
    executeRetirements(batch, [&](auto& fn)
    {
        fn();
        ensure_equals("lease count released before capture destroyed", leases, 0u);
        ensure("executed closure still owns validation pin", !held.expired());
#if LL_WINDOWS
        std::error_code error;
        std::filesystem::remove(cache.root / "orphan.mp4", error);
        ensure("negative control: executed capture denies delete", bool(error));
#endif
    });
    ensure("production batch destroys every capture before sweeping", held.expired());
    ensure("quit sweep unlinks playing-at-quit file", std::filesystem::remove(cache.root / "orphan.mp4"));
    ensure("maintenance exits only after that pass", finishShutdownMaintenance(false, true, false, false));
}

template<> template<> void media_file_object::test<54>()
{
    set_test_name("maintenance observes child death without parent idle or message pumping");
    auto fence = std::make_shared<LLPluginFileClose>();
    auto held = std::make_shared<int>(0);
    std::weak_ptr<int> lease = held;
    DeferredClose retirement(held, fence);
    held.reset();
#if LL_WINDOWS
    struct Child
    {
        PROCESS_INFORMATION info{};
        ~Child()
        {
            if (info.hProcess) { TerminateProcess(info.hProcess, 1); CloseHandle(info.hProcess); }
            if (info.hThread) CloseHandle(info.hThread);
        }
    } child;
    wchar_t system[MAX_PATH]{};
    ensure("system directory", GetSystemDirectoryW(system, MAX_PATH) != 0);
    const std::wstring executable = std::wstring(system) + L"\\cmd.exe";
    std::wstring command = L"\"" + executable + L"\" /d /c exit 0";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    ensure("disposable child starts suspended", CreateProcessW(executable.c_str(), command.data(),
        nullptr, nullptr, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child.info) != FALSE);
    fence->observeProcessHandle(child.info.hProcess); // Same duplication as parent.
    ensure("live child retains file", !retirement.poll() && !lease.expired());
    ensure("child resumed", ResumeThread(child.info.hThread) != DWORD(-1));
    ensure("child exits without main event loop", WaitForSingleObject(child.info.hProcess, 2000) == WAIT_OBJECT_0);
    // Destroy the original handle before polling to verify independent lifetime.
    CloseHandle(child.info.hProcess); child.info.hProcess = nullptr;
#else
    std::atomic<bool> exited{false};
    fence->observeExitWith([&]() { return exited.load(); });
    ensure("live child retains file", !retirement.poll() && !lease.expired());
    exited = true;
#endif
    bool closed = false;
    std::thread worker([&]() { closed = retirement.poll(); });
    worker.join();
    ensure("worker alone acknowledges death and releases lease", closed && lease.expired());
}

template<> template<> void media_file_object::test<55>()
{
    set_test_name("quit drain exits on no useful deletion work and retries only newly released work");
    ensure("Keep ON returns immediately even with a private player", finishShutdownMaintenance(true, false, true, true));
    ensure("Keep OFF waits for deletable entries", !finishShutdownMaintenance(false, true, false, true));
    ensure("Keep OFF returns when deletable set empty", finishShutdownMaintenance(false, true, false, false));
    ensure("live process still owns retirement", !finishShutdownMaintenance(false, true, true, false));
    ensure("local-only player never delays cache drain", !shutdownRetirementPending(false, true, false));
    ensure("cached player still protects deletable work", shutdownRetirementPending(false, true, true));
    ensure("unfinished transfers can produce new work", !finishShutdownMaintenance(false, false, false, false));
    const int64_t retry = 60000;
    unsigned passes = 0;
    for (int64_t now = 20; now < 2000; now += 20)
        passes += cacheMaintenanceDue(now, retry, false, false);
    ensure_equals("locked file never retries at 50 Hz", passes, 0u);
    ensure("new quit request gets one pass", cacheMaintenanceDue(20, retry, true, false));
    ensure("newly released lease gets one immediate pass", cacheMaintenanceDue(40, retry, false, true));
    ensure("same release never re-forces clear", !cacheMaintenanceDue(60, retry, false, false));
    ensure("ordinary backoff eventually retries", cacheMaintenanceDue(retry, retry, false, false));
    CacheOrphans orphans;
    orphans.files["locked"] = 1;
    for (unsigned i = 0; i < 300; ++i) orphans.files["video" + std::to_string(i)] = 1;
    std::set<std::string> attempted;
    unsigned lockedAttempts = 0, removed = 0, batches = 0;
    bool pending;
    do
    {
        ++batches;
        pending = orphans.quitBatch(attempted, [&](const std::string& name)
        {
            if (name == "locked") { ++lockedAttempts; return false; }
            ++removed; return true;
        });
    } while (pending && batches < 10);
    ensure_equals("all deletable entries drain across bounded batches", removed, 300u);
    ensure_equals("large quit sweep uses bounded batches", batches, 3u);
    ensure_equals("undeletable file gets one attempt across continuations", lockedAttempts, 1u);
    ensure_equals("only undeletable work remains", orphans.files.size(), size_t(1));
}

template<> template<> void media_file_object::test<56>()
{
    set_test_name("cache ownership wait has its own deadline before transfer or filesystem I/O starts");
    const int64_t queued = 100, started = 0, ioDeadline = 0;
    const int64_t ownerDeadline = queued + CACHE_OWNER_WAIT_MS;
    ensure("no transfer deadline yet", !totalExpired(false, started, ownerDeadline));
    ensure("no filesystem deadline yet", !deadlineExpired(ioDeadline, ownerDeadline));
    ensure("owner wait permits bounded recovery", !deadlineExpired(ownerDeadline, ownerDeadline - 1));
    ensure("owner wait terminalizes independently", deadlineExpired(ownerDeadline, ownerDeadline));
    ensure_equals("typed timeout status", cacheFailureStatus(CacheFailure::IO), std::string("Video cache unavailable: cache I/O failed"));
}

template<> template<> void media_file_object::test<57>()
{
    set_test_name("404 then invalid committed URL: newest commit wins both surfaces without stopping playback");
    const std::string runtime = std::string(FAILED) + " (HTTP 404)";
    const auto failed = nextMediaStatusSequence();
    std::string error;
    unsigned replacements = 0;
    ensure("invalid typed URL refused", !commitVideoDraft("https://invalid", error, true,
        [&](const Descriptor&) { ++replacements; return true; }));
    const auto edited = nextMediaStatusSequence();
    const auto source = mediaSourceStatus(runtime, error, failed, edited);
    const auto tile = "Error: " + mediaSourceStatus(runtime, error, failed, edited);
    ensure_equals("fresh edit outranks old 404", source, std::string(UNSUPPORTED));
    ensure_equals("tile uses same selected event plus Error prefix", tile, "Error: " + source);
    ensure_equals("invalid draft never replaces/stops the player", replacements, 0u);
    ensure_equals("a later runtime failure outranks cached edit", mediaSourceStatus(runtime, error, nextMediaStatusSequence(), edited), runtime);
    ensure_equals("a later valid validation also outranks stale failure",
        mediaSourceStatus(runtime, "Set. Press Play to start it.", failed, nextMediaStatusSequence()), std::string("Set. Press Play to start it."));
}

template<> template<> void media_file_object::test<58>()
{
    set_test_name("cache error categories remain distinct and only completed owner purge is acknowledged");
    ensure("generic EIO remains IO", cacheFailureFromError(std::error_code(EIO, std::generic_category()), CacheFailure::IO) == CacheFailure::IO);
    ensure("errno access denied", cacheFailureFromError(std::error_code(EACCES, std::generic_category()), CacheFailure::IO) == CacheFailure::ACCESS);
    ensure("errno no space", cacheFailureFromError(std::error_code(ENOSPC, std::generic_category()), CacheFailure::IO) == CacheFailure::NO_SPACE);
#if LL_WINDOWS
    ensure("Win32 access denied", cacheFailureFromError(std::error_code(ERROR_ACCESS_DENIED, std::system_category()), CacheFailure::IO) == CacheFailure::ACCESS);
    ensure("Win32 disk full", cacheFailureFromError(std::error_code(ERROR_DISK_FULL, std::system_category()), CacheFailure::IO) == CacheFailure::NO_SPACE);
    ensure("Win32 handle disk full", cacheFailureFromError(std::error_code(ERROR_HANDLE_DISK_FULL, std::system_category()), CacheFailure::IO) == CacheFailure::NO_SPACE);
#endif
    ensure("non-owner cannot consume purge", !acknowledgeStartupPurge(false, false, false, true, true));
    ensure("pending cleanup preserves purge", !acknowledgeStartupPurge(true, true, false, true, true));
    ensure("degraded cleanup acknowledges purge", acknowledgeStartupPurge(true, false, true, true, true));
    ensure("leftovers preserve purge", !acknowledgeStartupPurge(true, false, false, false, true));
    ensure("successful owner clears purge", acknowledgeStartupPurge(true, false, false, true, true));
}

template<> template<> void media_file_object::test<59>()
{
    set_test_name("LOCKED plus native sharing violation preserves IN_USE and the exact second-viewer status");
    const auto failure = cacheFailureFromError(std::error_code(32, std::system_category()), CacheFailure::IN_USE);
    ensure("semantic LOCKED survives native category mapping", failure == CacheFailure::IN_USE);
    ensure_equals("spec exact status", cacheFailureStatus(failure), std::string("Video downloads are in use by another open viewer"));
    ensure("tile treats exact lock status as a runtime failure", mediaRuntimeFailure(cacheFailureStatus(failure)));
    ensure("real access refusal remains access", cacheFailureFromError(
        std::make_error_code(std::errc::permission_denied), CacheFailure::IO) == CacheFailure::ACCESS);
}

template<> template<> void media_file_object::test<60>()
{
    set_test_name("Clear retries three times within one second and a stalled Clear wait has a deadline");
    for (unsigned failCount : {1u, 3u})
    {
        bool clear = true, cleanup = false, degraded = false;
        unsigned failures = 0, attempts = 0, drops = 0;
        int64_t now = 0, retry = 0;
        awaitCacheClear(clear, CACHE_CLEAR_WAIT_MS, [&]() { return now; }, []() {}, [&]()
        {
            if (cacheMaintenanceDue(now, retry, false, false))
            {
                try
                {
                    maintainCache(clear, cleanup, failures, degraded, [&](bool)
                    {
                        retry = now + 60000; // production inventory's ordinary backoff
                        if (++attempts <= failCount) throw std::runtime_error(FAILED);
                    }, []() { return false; }, [&]() { ++drops; }, []() { return true; });
                }
                catch (...) { retry = clearFailureRetry(now, retry, clear, failures, false); }
            }
            now += 20;
        });
        ensure("Clear resolved in less than one second", now < 1000);
        ensure_equals("one transient failure recovers; persistent failure gets three attempts", attempts, failCount == 1 ? 2u : 3u);
        ensure_equals("only persistent failure drops hits", drops, failCount == 1 ? 0u : 1u);
        ensure("persistent failure degrades", degraded == (failCount == 3));
        ensure_equals("resolved/degraded Clear retains ordinary backoff", clearFailureRetry(now, retry, clear, failures, false), retry);
    }
    ensure_equals("quit never hammers failed names", clearFailureRetry(0, 60000, true, 1, true), int64_t(60000));
    bool stalled = true, timedOut = false;
    int64_t now = 100;
    const auto deadline = now + CACHE_CLEAR_WAIT_MS;
    try
    {
        awaitCacheClear(stalled, deadline, [&]() { return now; }, []() {}, [&]() { now += 20; });
    }
    catch (const std::runtime_error& error)
    { timedOut = std::string(error.what()) == CLEAR_TIMEOUT; }
    ensure("stalled maintenance terminalizes with a clear-specific status", timedOut && stalled);
    ensure_equals("wait does not exceed deadline", now, deadline);
    ensure("timeout visible as runtime failure", mediaRuntimeFailure(CLEAR_TIMEOUT));
}

template<> template<> void media_file_object::test<61>()
{
    set_test_name("cancelled validation pin schedules a final sweep; degraded purge is consumed once");
    struct Record { std::atomic<unsigned> leases{1}; };
    auto entry = std::make_shared<Record>();
    std::atomic<uint64_t> released{0};
    try
    {
        CacheValidationPin<Record> pin{entry, released}; // production validation guard
        throw std::runtime_error(FAILED); // cancelled preflight unwinds without adoption
    }
    catch (const std::runtime_error&) {}
    ensure_equals("unadopted validation releases its lease", entry->leases.load(), 0u);
    ensure_equals("unadopted validation publishes newly released work", released.load(), uint64_t(1));
    ensure("backoff cannot hide the final sweep", cacheMaintenanceDue(20, 60000, false, released != 0));
    ensure("new deletion work prevents final-pass exit", !finishShutdownMaintenance(false, true, false, released != 0));
    entry->leases = 1;
    { CacheValidationPin<Record> pin{entry, released}; pin.adopted = true; }
    ensure_equals("adopted pin is retained for real lease retirement", entry->leases.load(), 1u);
    ensure_equals("adoption does not fake a release", released.load(), uint64_t(1));
    bool purgePending = true;
    if (acknowledgeStartupPurge(true, false, true, false, true)) purgePending = false;
    ensure("known undeletable leftovers acknowledge degraded owner purge", !purgePending);
    ensure("Keep ON next startup preserves the remaining cache", !clearCacheAtStartup(true, true, purgePending));
    ensure("non-owner still cannot acknowledge degraded purge", !acknowledgeStartupPurge(false, false, true, false, true));
    ensure("unresolved Clear still cannot acknowledge purge", !acknowledgeStartupPurge(true, true, true, false, true));
}

template<> template<> void media_file_object::test<62>()
{
    set_test_name("production shutdown begin/drain bypasses worker wait for Keep ON and bounds Keep OFF");
    for (unsigned blocked : {1u, 3u, 8u}) // maintenance, preflight/download, blocked filesystem workers
    {
        ShutdownDrain shutdown;
        std::atomic<unsigned> active{blocked};
        int64_t now = 100;
        unsigned ticks = 0, sleeps = 0, clocks = 0;
        auto clock = [&]() { ++clocks; return now; };
        auto tick = [&]() { ++ticks; }; // blocked jobs never become reaped
        auto sleep = [&]() { ++sleeps; now += 10; };
        shutdown.begin(true, now);
        ensure("outstanding jobs remain for jobsRemain/forced-exit safeguard", !shutdown.drain(active, clock, tick, sleep));
        ensure("drain begins after player retirement", shutdown.draining);
        ensure_equals("Keep ON never sleeps for workers", sleeps, 0u);
        ensure_equals("Keep ON never waits on a drain deadline", clocks, 0u);
        ensure_equals("Keep ON bypasses even the worker polling pass", ticks, 0u);
        ensure_equals("Keep ON returns at once", now, int64_t(100));
        ensure_equals("outstanding jobs are never marked drained", active.load(), blocked);
        shutdown.begin(false, now);
        ensure("Keep OFF reports blocked jobs after the bounded drain", !shutdown.drain(active, clock, tick, sleep));
        ensure_equals("Keep OFF retains the single two-second budget", now, int64_t(2100));
        active = 0;
        ensure("genuinely reaped workers permit normal teardown", shutdown.drain(active, clock, tick, sleep));
    }
}

template<> template<> void media_file_object::test<63>()
{
    set_test_name("Clear covers more than three batches before acknowledgement; only failed unlinks earn strikes");
    for (bool locked : {false, true})
    {
        CacheOrphans orphans;
        std::map<std::string,uint64_t> disk;
        for (unsigned i = 0; i < 600; ++i) disk["child-" + std::to_string(i)] = 1;
        if (locked) disk["aaa-locked"] = 7;
        bool clear = true, cleanup = false, degraded = false;
        unsigned failures = 0, passes = 0, drops = 0;
        size_t leased = 0;
        std::set<std::string> attempted;
        int64_t now = 0;
        while (clear && passes < 30)
        {
            ++passes;
            maintenanceItem([&]()
            {
                maintainCache(clear, cleanup, failures, degraded, [&](bool)
                {
                    orphans.unknown = true;
                    orphans.inventoried(disk); // same complete-inventory publication as production
                    orphans.nextRetry = now;
                    unsigned batch = 0;
                    orphans.retry(now, [&](const std::string& name)
                    {
                        ++batch;
                        attempted.insert(name);
                        if (name == "aaa-locked") return false;
                        disk.erase(name);
                        return true;
                    });
                    ensure("each pass bounded", batch <= 128);
                    if (!orphans.failed.empty()) throw std::runtime_error(FAILED);
                }, [&]() { return leased != 0; }, [&]() { ++drops; },
                    [&]() { return orphans.covered() && !leased; });
            }, []() {});
            if (passes <= 3)
            {
                ensure("three batches cannot acknowledge 600 children", clear);
                ensure("purge marker stays pending", !acknowledgeStartupPurge(true, clear, degraded, disk.empty(), orphans.covered()));
            }
            if (locked && passes == 3)
                ensure("degraded Play opens before attempt coverage completes", degraded && orphans.unattempted() != 0);
            for (const auto& child : orphans.files)
                if (!attempted.count(child.first)) ensure("unattempted has no deletion strikes", !orphans.failed.count(child.first));
            orphans.nextRetry = clearFailureRetry(now, orphans.nextRetry, clear, failures, false);
            if (clear) ensure_equals("coverage continues at fast cadence", orphans.nextRetry, now + 250);
            now += 250;
        }
        ensure("all children attempted", attempted.size() == (locked ? 601u : 600u));
        ensure("Clear eventually completes", !clear);
        ensure("purge completes only with covered children", acknowledgeStartupPurge(true, clear, degraded, disk.empty(), orphans.covered()));
        ensure_equals("only real failures degrade", drops, locked ? 1u : 0u);
        if (locked) ensure_equals("locked child has three actual failures", orphans.failed.at("aaa-locked"), 3u);
        ensure_equals("deletable children all removed", disk.size(), locked ? size_t(1) : size_t(0));
    }
}

template<> template<> void media_file_object::test<64>()
{
    set_test_name("failed inventories never acknowledge Clear or purge; degraded Play proceeds and leases retain completion");
    CacheOrphans orphans;
    bool clear = true, cleanup = false, degraded = false;
    unsigned failures = 0, drops = 0;
    size_t leased = 1;
    int64_t now = 0;
    for (unsigned i = 0; i < 5; ++i)
    {
        maintenanceItem([&]()
        {
            maintainCache(clear, cleanup, failures, degraded, [&](bool)
            { orphans.unknown = true; throw std::runtime_error(FAILED); },
                [&]() { return leased != 0; }, [&]() { ++drops; },
                [&]() { return orphans.covered() && !leased; });
        }, []() {});
        ensure("inventory failure preserves Clear", clear);
        ensure("even a caller with no pending bit cannot acknowledge unknown inventory",
            !acknowledgeStartupPurge(true, false, degraded, false, orphans.covered()));
        ensure("inventory failure creates no deletion strikes", orphans.failed.empty());
        ensure_equals("inventory failure retains fast retry", clearFailureRetry(now, 60000, clear, failures, false), now + 250);
        now += 250;
    }
    ensure_equals("degradation logged once", drops, 1u);
    ensure("Play can bypass unresolved degraded Clear", degraded && clear);
    unsigned waits = 0;
    awaitCacheClearWhile([&]() { return clear && !degraded; }, CACHE_CLEAR_WAIT_MS,
        [&]() { return now; }, []() {}, [&]() { ++waits; });
    ensure_equals("degraded Play does not wait for acknowledgement", waits, 0u);
    orphans.inventoried({});
    maintainCache(clear, cleanup, failures, degraded, [](bool clearing)
        { ensure("degraded continuation does not clear newly downloaded hits", !clearing); },
        [&]() { return leased != 0; }, []() {}, [&]() { return orphans.covered() && !leased; });
    ensure("complete inventory alone cannot release retained lease", clear);
    leased = 0;
    maintainCache(clear, cleanup, failures, degraded, [](bool) {}, []() { return false; }, []() {},
        [&]() { return orphans.covered() && !leased; });
    ensure("successful inventory and lease release allow completion", !clear);
    ensure("owner can acknowledge now", acknowledgeStartupPurge(true, clear, degraded, true, orphans.covered()));
}

template<> template<> void media_file_object::test<65>()
{
    set_test_name("degraded log is one summary with at most three anonymous examples");
    CacheOrphans orphans;
    for (unsigned i = 0; i < 1000; ++i)
    {
        const auto key = std::string(64, 'a') + "." + std::to_string(i) + ".mp4";
        orphans.files[key] = 42;
        if (i < 400) orphans.failed[key] = 1;
    }
    orphans.unknown = true;
    const auto log = cacheClearSummary(orphans, 5);
    ensure("counts distinguish failures, untouched children and leases", log.find("failed=400 unattempted=600 leased=5 inventory_unknown=1") != std::string::npos);
    ensure("three examples supplied", log.find("example[3]=") != std::string::npos);
    ensure("fourth example omitted", log.find("example[4]") == std::string::npos);
    ensure("no cache key or filename leaks", log.find(std::string(64, 'a')) == std::string::npos && log.find(".mp4") == std::string::npos);
    ensure("single bounded line", log.find('\n') == std::string::npos && log.size() < 300);
    CacheOrphans empty;
    ensure("empty inventory supplies no examples", cacheClearSummary(empty, 2).find("example[") == std::string::npos);
    empty.files["private-path"] = 1;
    const auto one = cacheClearSummary(empty, 0);
    ensure("small inventory only supplies available examples", one.find("example[1]=unattempted") != std::string::npos && one.find("example[2]") == std::string::npos);
    ensure("arbitrary names excluded", one.find("private-path") == std::string::npos);
}

}

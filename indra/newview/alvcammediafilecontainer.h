/** Bounded metadata byte gate plus structural reference walk; mdat stays opaque. */
#ifndef AL_VCAM_MEDIA_FILE_CONTAINER_H
#define AL_VCAM_MEDIA_FILE_CONTAINER_H
#include "alvcammediafilepolicy.h"
#include "alvcammediafilelifecycle.h"
#include <array>
#include <stdexcept>
#include <map>
namespace ALMediaFile
{
// Reader(offset, destination, count) must read exactly count bytes or throw.
// All arithmetic is checked against the enclosing element and exact file size.
template<class Reader> class ContainerGate
{
public:
    ContainerGate(Reader& read, uint64_t size) : mRead(read), mSize(size) {}
    void check(const std::string& ext)
    {
        if (ext == "mkv" || ext == "webm") ebml(0, mSize, 0, false);
        else { bmff(0, mSize, 0); checkOffsets(); }
    }
private:
    Reader& mRead;
    uint64_t mSize;
    unsigned mElements = 0;
    uint64_t mSegmentBase = 0;
    bool mSegmentSeen = false;
    std::map<uint64_t,uint64_t> mSegmentElements;
    std::vector<std::pair<uint64_t,uint64_t>> mSeeks;
    std::map<uint64_t,std::string> mRoots;
    std::vector<std::pair<uint64_t,std::string>> mBoxSeeks;
    std::map<uint64_t,bool> mIndexHeaders;
    uint64_t mRootEnd = 0;
    bool mFragmented = false, mUnusualMdat = false, mChapterReference = false;
    enum class BmffContext { OTHER, MOVIE, TRACK, TRACK_REFS };
    uint64_t number(uint64_t offset, unsigned width, uint64_t end)
    {
        bound(offset, width, end);
        std::array<unsigned char,8> b{}; mRead(offset, b.data(), width);
        uint64_t value = 0;
        for (unsigned i = 0; i < width; ++i) value = value << 8 | b[i];
        return value;
    }
    uint64_t add(uint64_t offset, uint64_t delta)
    { if (offset > mSize || delta > mSize - offset) fail(); return offset + delta; }
    void checkOffsets()
    {
        // ProbeIndex reads this trailer independently of the structural walk.
        if (mSize >= 16)
        {
            std::array<unsigned char,16> tail{}; mRead(mSize-16, tail.data(), tail.size());
            if (be32(tail.data()) == 16 && be32(tail.data()+4) == 0x6d66726f)
            {
                const auto size = be32(tail.data()+12);
                if (size > mSize) fail();
                mBoxSeeks.emplace_back(mSize-size, "mfra");
            }
        }
        for (const auto& seek : mBoxSeeks)
        {
            const auto found = mRoots.find(seek.first);
            if (found == mRoots.end() || found->second != seek.second) fail();
        }
        // A plausible index hidden in a container we do not walk is refused.
        for (const auto& header : mIndexHeaders) if (!header.second) fail();
        // DemuxFrag's post-mdat seek uses the 32-bit size, not largesize/EOF.
        if (mFragmented && mUnusualMdat) fail();
        // LoadChapterApple leaves the stream at sample data; DemuxFrag can
        // parse boxes there before reaching its post-mdat position restore.
        if (mFragmented && mChapterReference) fail();
    }
    void fragmentIndex(const std::string& type, uint64_t offset, uint64_t content, uint64_t end)
    {
        mIndexHeaders[offset] = true;
        const auto version = number(content, 1, end);
        if (version > 1) fail();
        if (type == "sidx")
        {
            const unsigned width = version ? 8 : 4;
            auto position = add(end, number(content+12+width, width, end));
            auto cursor = content+12+2*width;
            const auto count = number(cursor+2, 2, end); cursor += 4;
            for (uint64_t i = 0; i < count; ++i, cursor += 12)
            {
                element(0); bound(cursor, 12, end);
                const auto reference = number(cursor, 4, end);
                // Hierarchical references are unsupported; do not guess a target.
                if (reference & 0x80000000) fail();
                mBoxSeeks.emplace_back(position, "moof");
                position = add(position, reference);
            }
            if (cursor != end) fail();
        }
        else
        {
            const auto lengths = number(content+8, 4, end);
            if (lengths & ~uint64_t(63)) fail();
            const auto count = number(content+12, 4, end);
            const unsigned width = version ? 8 : 4;
            const auto stride = 2*width + 3 + ((lengths>>4)&3) + ((lengths>>2)&3) + (lengths&3);
            auto cursor = content+16;
            for (uint64_t i = 0; i < count; ++i, cursor += stride)
            {
                element(0); bound(cursor, stride, end);
                mBoxSeeks.emplace_back(number(cursor+width, width, end), "moof");
            }
            if (cursor != end) fail();
        }
    }
    void bound(uint64_t offset, uint64_t count, uint64_t end)
    { if (end > mSize || offset > end || count > end - offset) fail(); }
    [[noreturn]] static void fail() { throw std::runtime_error(NOT_VIDEO); }
    void element(unsigned depth)
    { if (depth > 32 || ++mElements > 1000000) fail(); }
    void scanMetadata(uint64_t offset, uint64_t end, bool hiddenMovie, bool padding = false)
    {
        // Refuse oversized metadata rather than silently skipping any of it.
        // Carry the rolling FourCC across reads, including unaligned markers.
        if (end - offset > 64 * 1024 * 1024) fail();
        std::array<unsigned char,65536> bytes{};
        uint64_t window = 0;
        unsigned seen = 0;
        while (offset < end)
        {
            const auto count = static_cast<size_t>(std::min<uint64_t>(bytes.size(), end - offset));
            mRead(offset, bytes.data(), count);
            for (size_t i = 0; i < count; ++i)
            {
                window = (window << 8) | bytes[i];
                if (seen < 8) ++seen;
                if (seen < 8) continue;
                const auto code = uint32_t(window);
                const auto position = offset+i+1;
                const auto size = uint32_t(window >> 32);
                // Root-bound size plausibility avoids treating ordinary text
                // as a box while retaining unaligned/chunk-spanning headers.
                if (size != 0 && size != 1 &&
                    (size < 8 || size > mRootEnd - (position-8))) continue;
                if (seen >= 8 && (code == 0x73696478 || code == 0x74667261))
                    mIndexHeaders.emplace(position-8, false);
                if (code == 0x636d6f76 || code == 0x726d7261 || code == 0x726d6461 ||
                    code == 0x72647266 || code == 0x666f6f76 || code == 0x64636f6d || code == 0x636d7664) fail();
                if ((hiddenMovie || padding) &&
                    (code == 0x6d766864 || code == 0x6d6f6f76 || code == 0x7472616b)) fail();
                // Retain the existing conservative padding-reference refusal.
                if (padding && code == 0x64726566) fail();
            }
            offset += count;
        }
    }
    void bmff(uint64_t offset, uint64_t end, unsigned depth, bool udta = false,
              BmffContext context = BmffContext::OTHER)
    {
        while (offset < end)
        {
            element(depth);
            std::array<unsigned char,16> b{};
            // Only a short fragment at the physical file end is tolerated.
            // Scan it too: a malformed tail cannot erase an earlier refusal.
            if (!depth && end - offset < 8)
            {
                mRootEnd = end;
                scanMetadata(offset >= 4 ? offset-4 : offset, end, false);
                return;
            }
            // QuickTime udta may end with a lone 32-bit zero, not a box.
            if (udta && end - offset == 4)
            { mRead(offset, b.data(), 4); if (be32(b.data()) == 0) return; fail(); }
            bound(offset, 8, end); mRead(offset, b.data(), 8);
            uint64_t size = be32(b.data()), head = 8;
            const std::string type(reinterpret_cast<const char*>(b.data()+4), 4);
            if (size == 1)
            { bound(offset, 16, end); mRead(offset+8, b.data()+8, 8); size = be32(b.data()+8) << 32 | be32(b.data()+12); head = 16; }
            else if (!size) size = end - offset;
            if (size < head) fail();
            bound(offset, size, end);
            const auto next = offset + size;
            const auto content = offset + head;
            if (!depth)
            {
                mRoots[offset] = type; mRootEnd = next;
                if (type == "mdat" && (head != 8 || !be32(b.data()))) mUnusualMdat = true;
            }
            if (type == "mvex" || type == "moof") mFragmented = true;
            if (context == BmffContext::TRACK_REFS && type == "chap") mChapterReference = true;
            if (!depth && type != "mdat")
                scanMetadata(offset, next, type != "moov" && type != "ftyp" && type != "meta");
            if (type == "free" || type == "skip" || type == "wide")
                scanMetadata(content, next, false, true);
            if (type == "sidx" || type == "tfra") fragmentIndex(type, offset, content, next);
            else if (type == "dref")
            {
                bound(content, 8, next); mRead(content, b.data(), 8);
                const auto count = be32(b.data()+4);
                if (count > 1024) fail();
                uint64_t entry = content + 8;
                for (uint64_t i = 0; i < count; ++i)
                {
                    bound(entry, 12, next); mRead(entry, b.data(), 12);
                    // Self-contained ISO URL and QuickTime alias entries have
                    // flag 1 and no external path payload.
                    const std::string kind(reinterpret_cast<const char*>(b.data()+4),4);
                    if (be32(b.data()) != 12 || (kind != "url " && kind != "alis") || be32(b.data()+8) != 1) fail();
                    entry += 12;
                }
                if (entry != next) fail();
            }
            else if (type == "moov" || type == "trak" || type == "mdia" || type == "minf" || type == "dinf" || type == "stbl" || type == "edts" || type == "udta" || type == "moof" || type == "traf" || type == "mvex" || type == "mfra" || (type == "tref" && context == BmffContext::TRACK))
            {
                auto childContext = BmffContext::OTHER;
                if (!depth && type == "moov") childContext = BmffContext::MOVIE;
                else if (context == BmffContext::MOVIE && type == "trak") childContext = BmffContext::TRACK;
                else if (context == BmffContext::TRACK && type == "tref") childContext = BmffContext::TRACK_REFS;
                bmff(content, next, depth+1, type == "udta", childContext);
            }
            else if (type == "meta")
            {
                bound(content, 4, next);
                bool quicktime = false;
                if (next - content >= 8)
                {
                    mRead(content, b.data(), 8);
                    auto childSize = be32(b.data());
                    const bool handler = std::string(reinterpret_cast<const char*>(b.data()+4),4) == "hdlr";
                    if (handler && childSize == 1 && next - content >= 16)
                    { mRead(content+8, b.data()+8, 8); childSize = be32(b.data()+8) << 32 | be32(b.data()+12); if (childSize < 16) fail(); }
                    else if (!childSize) childSize = next - content;
                    quicktime = childSize >= 8 && childSize <= next - content &&
                        handler;
                }
                bmff(content + (quicktime ? 0 : 4), next, depth+1);
            }
            // Unknown leaf structure is opaque, but its bytes have already
            // passed the enclosing root's metadata scan.
            offset = next;
        }
    }
    struct EbmlHeader { uint64_t id = 0, size = 0, content = 0; bool unknown = false; };
    EbmlHeader header(uint64_t offset, uint64_t end)
    {
        EbmlHeader result;
        unsigned char first = 0;
        bound(offset, 1, end); mRead(offset, &first, 1);
        unsigned width = 1, mask = 0x80;
        while (width <= 4 && !(first & mask)) { ++width; mask >>= 1; }
        if (width > 4) fail();
        std::array<unsigned char,8> bytes{};
        bound(offset, width, end); mRead(offset, bytes.data(), width);
        for (unsigned i = 0; i < width; ++i) result.id = result.id << 8 | bytes[i];
        offset += width;
        bound(offset, 1, end); mRead(offset, &first, 1);
        width = 1; mask = 0x80;
        while (width <= 8 && !(first & mask)) { ++width; mask >>= 1; }
        if (width > 8) fail();
        bound(offset, width, end); mRead(offset, bytes.data(), width);
        result.size = bytes[0] & (mask - 1);
        bool allOnes = result.size == mask - 1;
        for (unsigned i = 1; i < width; ++i) { result.size = result.size << 8 | bytes[i]; allOnes = allOnes && bytes[i] == 255; }
        result.content = offset + width; result.unknown = allOnes;
        return result;
    }
    static bool segmentChild(uint64_t id)
    {
        return id == 0x1549a966 || id == 0x1654ae6b || id == 0x1f43b675 ||
            id == 0x1c53bb6b || id == 0x1043a770 || id == 0x1941a469 ||
            id == 0x1254c367 || id == 0x114d9b74;
    }
    uint64_t unsignedValue(const EbmlHeader& h, uint64_t next)
    {
        if (h.unknown || !h.size || h.size > 8) fail();
        std::array<unsigned char,8> bytes{};
        bound(h.content, h.size, next); mRead(h.content, bytes.data(), static_cast<size_t>(h.size));
        uint64_t value = 0;
        for (uint64_t i = 0; i < h.size; ++i) value = value << 8 | bytes[static_cast<size_t>(i)];
        return value;
    }
    void seekHead(uint64_t offset, uint64_t end, unsigned depth)
    {
        while (offset < end)
        {
            element(depth);
            const auto entry = header(offset, end);
            if (entry.unknown) fail(); bound(entry.content, entry.size, end);
            const auto next = entry.content + entry.size;
            if (entry.id == 0x4dbb)
            {
                uint64_t id = 0, position = 0; bool havePosition = false;
                for (auto field = entry.content; field < next;)
                {
                    element(depth+1);
                    const auto h = header(field, next);
                    if (h.unknown) fail(); bound(h.content, h.size, next);
                    if (h.id == 0x53ab) { if (h.size > 4) fail(); id = unsignedValue(h, next); }
                    else if (h.id == 0x53ac) { position = unsignedValue(h, next); havePosition = true; }
                    field = h.content + h.size;
                }
                if (!id || !havePosition || position > mSize - mSegmentBase) fail();
                mSeeks.emplace_back(mSegmentBase + position, id);
            }
            offset = next;
        }
    }
    uint64_t ebml(uint64_t offset, uint64_t end, unsigned depth, bool unknownCluster, bool segment = false)
    {
        while (offset < end)
        {
            element(depth);
            const auto h = header(offset, end);
            if (unknownCluster && segmentChild(h.id)) return offset;
            if (segment) mSegmentElements[offset] = h.id;
            if (h.id == 0x3cb923 || h.id == 0x3eb923 || h.id == 0x3c83ab ||
                h.id == 0x3e83bb || h.id == 0x6e67 || h.id == 0x4444) fail();
            uint64_t next = end;
            if (!h.unknown) { bound(h.content, h.size, end); next = h.content + h.size; }
            if (h.id == 0x18538067) // Segment; unknown length is valid file video
            {
                if (mSegmentSeen) fail();
                mSegmentSeen = true; mSegmentBase = h.content;
                ebml(h.content, next, depth+1, false, true);
                // A seek table/cue must not hide metadata inside opaque frame
                // payload: every referenced target is a real Segment child.
                for (const auto& seek : mSeeks)
                {
                    const auto found = mSegmentElements.find(seek.first);
                    if (found == mSegmentElements.end() || found->second != seek.second) fail();
                }
            }
            else if (h.id == 0x1f43b675 && h.unknown)
                next = ebml(h.content, end, depth+1, true);
            else if (h.id == 0x114d9b74)
            { if (h.unknown || !mSegmentSeen) fail(); seekHead(h.content, next, depth+1); }
            else if (h.id == 0xf1)
            {
                const auto position = unsignedValue(h, next);
                if (!mSegmentSeen || position > mSize - mSegmentBase) fail();
                mSeeks.emplace_back(mSegmentBase + position, 0x1f43b675);
            }
            else if (h.id == 0x1a45dfa3 || h.id == 0x1549a966 || h.id == 0x1043a770 || h.id == 0x45b9 || h.id == 0xb6 || h.id == 0x1c53bb6b || h.id == 0xbb || h.id == 0xb7)
            {
                if (h.unknown) fail();
                ebml(h.content, next, depth+1, false);
            }
            else if (h.unknown) fail();
            // Known-sized Cluster, SimpleBlock/BlockGroup, codec private data,
            // attachments and unrelated metadata remain opaque encoded bytes.
            if (next <= offset) fail();
            offset = next;
        }
        return offset;
    }
};
template<class Reader> void checkContainer(Reader read, uint64_t size, const std::string& ext)
{ ContainerGate<Reader>(read, size).check(ext); }

// Exact-descriptor validation plus post-walk identity check. POSIX supplies
// fstat/pread adapters; fixtures can change bytes during the real walk.
template<class Reader, class Snapshot>
FileIdentity validateFile(Reader read, Snapshot snapshot, const std::string& ext,
    const FileIdentity* expected = nullptr)
{
    const auto before = snapshot();
    if (!before.size || (expected && !(*expected == before))) throw std::runtime_error(NOT_VIDEO);
    std::array<unsigned char,64> header{};
    const auto count = static_cast<size_t>(std::min<uint64_t>(header.size(), before.size));
    read(0, header.data(), count);
    if (!sniff(ext, header.data(), count, before.size)) throw std::runtime_error(NOT_VIDEO);
    checkContainer(read, before.size, ext);
    if (!(before == snapshot())) throw std::runtime_error(NOT_VIDEO);
    return before;
}
}
#endif

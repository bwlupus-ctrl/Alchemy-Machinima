/** Cache metadata recovery. Call only while holding the private cache lock. */
#ifndef AL_VCAM_MEDIA_FILE_CACHE_H
#define AL_VCAM_MEDIA_FILE_CACHE_H
#include "alvcammediafilepolicy.h"
#include <cerrno>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>
#if LL_WINDOWS
#include "llwin32headers.h"
#endif

namespace ALMediaFile
{
struct CacheIndexRecord
{
    std::string key, ext, mime;
    uint64_t id = 0, size = 0, lru = 0;
};
struct CacheIndexSnapshot
{
    std::vector<CacheIndexRecord> entries;
    bool recovered = false;
};
inline bool cacheIndexNumber(const std::string& text, uint64_t& number)
{
    const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
    return !text.empty() && result.ec == std::errc() && result.ptr == text.data() + text.size();
}
inline CacheIndexSnapshot readCacheIndex(const std::filesystem::path& path)
{
    CacheIndexSnapshot snapshot;
    // The reader belongs to this scope, never to the caller's sweep/replace.
    errno = 0;
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        const int error = errno;
        if (error != ENOENT) throw std::system_error(error ? error : EIO, std::generic_category());
        snapshot.recovered = true;
        return snapshot;
    }
    std::set<std::string> keys;
    std::string line;
    while (std::getline(in, line))
    {
        CacheIndexRecord entry;
        std::string id, size, lru, extra;
        std::istringstream row(line);
        if (!(row >> entry.key >> id >> entry.ext >> entry.mime >> size >> lru) || row >> extra ||
            entry.key.size() != 64 || entry.key.find_first_not_of("0123456789abcdef") != std::string::npos ||
            extension("x." + entry.ext) != entry.ext || !mimeAllowed(entry.mime) ||
            !cacheIndexNumber(id, entry.id) || !cacheIndexNumber(size, entry.size) ||
            !cacheIndexNumber(lru, entry.lru) || !entry.size || !keys.insert(entry.key).second)
        {
            snapshot.entries.clear(); // Never publish a valid prefix of corrupt/partial metadata.
            snapshot.recovered = true;
            return snapshot;
        }
        snapshot.entries.push_back(std::move(entry));
    }
    if (in.bad()) throw std::system_error(EIO, std::generic_category());
    return snapshot; // An empty index is valid.
}
inline void removeCacheIndexTemporary(const std::filesystem::path& root)
{
    // Remove the directory entry, never follow a stale temporary symlink.
    std::error_code error;
    std::filesystem::remove(root / "index.tmp", error);
    if (error) throw std::system_error(error);
}
inline void replaceCacheIndex(const std::filesystem::path& root)
{
#if LL_WINDOWS
    if (!MoveFileExW((root / "index.tmp").c_str(), (root / "index").c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const auto error = GetLastError(); // Capture before stack unwinding closes anything.
        throw std::system_error(static_cast<int>(error), std::system_category());
    }
#else
    std::filesystem::rename(root / "index.tmp", root / "index");
#endif
}
}
#endif

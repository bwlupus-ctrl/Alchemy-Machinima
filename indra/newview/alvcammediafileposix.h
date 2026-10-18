/** Local-only, no-follow POSIX opens shared by isolated I/O and the VLC child. */
#ifndef AL_VCAM_MEDIA_FILE_POSIX_H
#define AL_VCAM_MEDIA_FILE_POSIX_H
#if !LL_WINDOWS
#include "alvcammediafilepolicy.h"
#include "alvcammediafilecontainer.h"
#include "alvcammediafilelifecycle.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <array>
#include <cstring>
#include <chrono>
#if LL_DARWIN
#include <sys/mount.h>
#endif
namespace ALMediaFile
{
inline bool localMount(const std::string& path)
{
    size_t longest = 0;
    bool local = false;
#if LL_LINUX
    // Read the kernel's mount table, without probing any remote mount itself.
    std::ifstream mounts("/proc/self/mountinfo");
    std::string line;
    while (std::getline(mounts, line))
    {
        std::istringstream row(line);
        std::string id, parent, device, root, point, options, field, type;
        if (!(row >> id >> parent >> device >> root >> point >> options)) continue;
        while (row >> field && field != "-") {}
        if (!(row >> type)) continue;
        std::string decoded;
        for (size_t i = 0; i < point.size(); ++i)
            if (point[i] == '\\' && i + 3 < point.size() && point[i+1] >= '0' && point[i+1] <= '7' && point[i+2] >= '0' && point[i+2] <= '7' && point[i+3] >= '0' && point[i+3] <= '7')
            { decoded += static_cast<char>((point[i+1]-'0')*64 + (point[i+2]-'0')*8 + point[i+3]-'0'); i += 3; }
            else decoded += point[i];
        if ((decoded == "/" || path == decoded || path.rfind(decoded + "/", 0) == 0) && decoded.size() >= longest)
        {
            longest = decoded.size();
            local = type == "ext4" || type == "ext3" || type == "ext2" || type == "xfs" || type == "btrfs" || type == "tmpfs" || type == "overlay" || type == "zfs" || type == "vfat" || type == "exfat";
        }
    }
#else
    struct statfs* mounts = nullptr;
    const int count = getmntinfo(&mounts, MNT_NOWAIT);
    for (int i = 0; i < count; ++i)
    {
        const std::string point(mounts[i].f_mntonname);
        if ((point == "/" || path == point || path.rfind(point + "/", 0) == 0) && point.size() >= longest)
        { longest = point.size(); local = (mounts[i].f_flags & MNT_LOCAL) != 0; }
    }
#endif
    return longest && local;
}
inline int localOpen(const std::string& path, bool directory = false, bool create = false)
{
    if (path.empty() || path[0] != '/' || networkSpelling(path)) throw std::runtime_error(REMOTE);
    int dir = open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dir < 0) throw std::runtime_error(REMOTE);
    std::string prefix;
    for (size_t p = 1; p < path.size();)
    {
        const auto end = path.find('/', p);
        const auto part = path.substr(p, end - p);
        prefix += '/' + part;
        if (part.empty() || part == "." || part == ".." || !localMount(prefix)) { close(dir); throw std::runtime_error(REMOTE); }
        const bool isDir = directory || end != std::string::npos;
        if (create && isDir && mkdirat(dir, part.c_str(), 0700) && errno != EEXIST) { close(dir); throw std::runtime_error(FAILED); }
        const int next = openat(dir, part.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | (isDir ? O_DIRECTORY : 0));
        const auto error = errno;
        close(dir);
        if (next < 0) throw std::runtime_error(error == ENOENT ? MISSING : REMOTE);
        dir = next;
        if (end == std::string::npos) break;
        p = end + 1;
    }
    return dir;
}
inline FileIdentity fileIdentity(const struct stat& info)
{
#if LL_DARWIN
    const auto time = info.st_mtimespec;
#else
    const auto time = info.st_mtim;
#endif
    return {static_cast<uint64_t>(info.st_dev), static_cast<uint64_t>(info.st_ino),
        static_cast<uint64_t>(info.st_size), time.tv_sec, time.tv_nsec};
}
inline bool unchangedFile(int fd, const FileIdentity& expected)
{
    struct stat info{};
    return !fstat(fd, &info) && S_ISREG(info.st_mode) && fileIdentity(info) == expected;
}
inline int verifiedLocalFile(const std::string& uri, const std::string& ext, FileIdentity* validated = nullptr,
    const FileIdentity* expected = nullptr)
{
    std::string path;
    if (uri.rfind("file:///", 0) != 0 || !decode(uri.substr(7), path)) throw std::runtime_error(REMOTE);
    const int fd = localOpen(path);
    try
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        const auto identity = validateFile([fd, deadline](uint64_t offset, void* bytes, size_t count)
        {
            if (std::chrono::steady_clock::now() >= deadline || pread(fd, bytes, count, offset) != static_cast<ssize_t>(count)) throw std::runtime_error(NOT_VIDEO);
        }, [fd]()
        {
            struct stat info{};
            if (fstat(fd, &info) || !S_ISREG(info.st_mode) || info.st_size <= 0) throw std::runtime_error(NOT_VIDEO);
            return fileIdentity(info);
        }, ext, expected);
        if (validated) *validated = identity;
        return fd; // VLC reads this exact verified descriptor, never reopens path.
    }
    catch (...) { close(fd); throw; }
}
}
#endif
#endif

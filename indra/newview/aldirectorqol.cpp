#include "linden_common.h"

#include "aldirectorqol.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace
{
S32 editDistance(const std::string& left, const std::string& right, S32 ceiling)
{
    constexpr size_t MAX_FUZZY_TOKEN = 128;
    if (std::abs((S32)left.size() - (S32)right.size()) > ceiling)
        return ceiling + 1;
    if (left.size() >= MAX_FUZZY_TOKEN || right.size() >= MAX_FUZZY_TOKEN)
        return ceiling + 1;
    std::array<S32, MAX_FUZZY_TOKEN> previous{}, current{};
    for (S32 column = 0; column <= (S32)right.size(); ++column)
        previous[column] = column;
    for (S32 row = 1; row <= (S32)left.size(); ++row)
    {
        current[0] = row;
        S32 row_best = current[0];
        for (S32 column = 1; column <= (S32)right.size(); ++column)
        {
            current[column] = llmin(previous[column] + 1,
                                    llmin(current[column - 1] + 1,
                                          previous[column - 1] +
                                              (left[row - 1] == right[column - 1] ? 0 : 1)));
            row_best = llmin(row_best, current[column]);
        }
        if (row_best > ceiling)
            return ceiling + 1;
        previous.swap(current);
    }
    // previous holds the last DP row; its live width is right.size() + 1.
    return previous[right.size()];
}

bool isSubsequence(const std::string& needle, const std::string& haystack)
{
    size_t at = 0;
    for (char character : haystack)
        if (at < needle.size() && needle[at] == character)
            ++at;
    return at == needle.size();
}
} // namespace

S32 ALDirectorQol::fuzzyTokenScore(const std::string& query_value,
                                   const std::string& candidate_value)
{
    const std::string& query = query_value;
    const std::string& candidate = candidate_value;
    if (query.empty() || candidate.empty())
        return 0;
    if (query == candidate)
        return 120;
    if (candidate.compare(0, query.size(), query) == 0)
        return 105;
    if (candidate.find(query) != std::string::npos)
        return 90;
    if (query.size() >= 3 && isSubsequence(query, candidate))
        return 65 - llmin(20, (S32)candidate.size() - (S32)query.size());
    const S32 ceiling = query.size() < 5 ? 1 : 2;
    const S32 distance = editDistance(query, candidate, ceiling);
    return distance <= ceiling ? 58 - distance * 8 : 0;
}

S32 ALDirectorQol::searchTokenScore(const std::string& query,
                                    const std::string& candidate,
                                    SearchField field)
{
    static constexpr S32 WEIGHTS[] = { 80, 55, 45, 25, 90, 15, 5 };
    const S32 fuzzy = fuzzyTokenScore(query, candidate);
    return fuzzy ? WEIGHTS[static_cast<U32>(field)] + fuzzy : 0;
}

bool ALDirectorQol::searchResultBefore(S32 left_score, bool left_external,
                                       U32 left_order, S32 right_score,
                                       bool right_external, U32 right_order)
{
    if (left_score != right_score)
        return left_score > right_score;
    if (left_external != right_external)
        return !left_external;
    return left_order < right_order;
}

std::vector<LLUUID> ALDirectorQol::subjectAssignment(
    const std::vector<LLUUID>& roster_order, U32 limit)
{
    std::vector<LLUUID> result;
    result.reserve(llmin(limit, (U32)roster_order.size()));
    for (const LLUUID& id : roster_order)
    {
        if (id.notNull() && std::find(result.begin(), result.end(), id) == result.end())
            result.push_back(id);
        if (result.size() == limit)
            break;
    }
    return result;
}

namespace
{
std::string lowerAscii(std::string text)
{
    for (char& character : text)
        if (character >= 'A' && character <= 'Z')
            character = (char)(character - 'A' + 'a');
    return text;
}

bool allDigits(const std::string& text, size_t start, size_t count)
{
    if (start + count > text.size())
        return false;
    for (size_t index = start; index < start + count; ++index)
        if (text[index] < '0' || text[index] > '9')
            return false;
    return true;
}

bool endsWith(const std::string& text, const std::string& suffix)
{
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool containsInsensitive(const std::vector<std::string>& names, const std::string& name)
{
    const std::string wanted = lowerAscii(name);
    for (const std::string& candidate : names)
        if (lowerAscii(candidate) == wanted)
            return true;
    return false;
}

constexpr char LIVE_PREFIX[] = ".director_recovery";
constexpr char LIVE_KEPT_NAME[] = ".director_recovery_kept.xml";
constexpr char LIVE_KEPT_OLDER_TAG[] = ".director_recovery_kept.xml.kept-";
} // namespace

std::string ALDirectorQol::liveRecoveryFileName(S32 pid, S32 counter)
{
    std::string name = std::string(LIVE_PREFIX) + "-p" + std::to_string(pid);
    if (counter > 0)
        name += "-" + std::to_string(counter);
    return name + ".xml";
}

S32 ALDirectorQol::liveRecoveryPid(const std::string& file_name)
{
    // ".director_recovery-p<1..10 digits, no leading zero>[-<1..6 digits, no leading zero>].xml"
    const std::string name = lowerAscii(file_name);
    const std::string prefix = std::string(LIVE_PREFIX) + "-p";
    if (name.compare(0, prefix.size(), prefix) != 0 || !endsWith(name, ".xml") ||
        name.size() < prefix.size() + 1 + 4)
        return 0;
    const size_t begin = prefix.size();
    const size_t end = name.size() - 4;
    size_t digits = 0;
    while (begin + digits < end && name[begin + digits] >= '0' && name[begin + digits] <= '9')
        ++digits;
    if (digits == 0 || digits > 10 || name[begin] == '0')
        return 0;
    const size_t at = begin + digits;
    if (at != end)
    {
        const size_t count = end - at - 1; // the "-<n>" counter
        if (name[at] != '-' || end < at + 2 || count > 6 || !allDigits(name, at + 1, count) ||
            name[at + 1] == '0')
            return 0;
    }
    const unsigned long long pid = std::strtoull(name.substr(begin, digits).c_str(), nullptr, 10);
    return pid > (unsigned long long)kMaxLiveRecoveryPid ? 0 : (S32)pid;
}

std::string ALDirectorQol::ownLiveRecoveryName(S32 pid, const std::vector<std::string>& existing_live_names)
{
    for (S32 counter = 0;; ++counter)
    {
        const std::string candidate = liveRecoveryFileName(pid, counter);
        if (!containsInsensitive(existing_live_names, candidate))
            return candidate;
    }
}

std::string ALDirectorQol::liveRecoveryLockName(const std::string& live_file_name)
{
    return live_file_name + ".lock";
}

bool ALDirectorQol::isLiveRecoveryLockFile(const std::string& file_name)
{
    const std::string suffix = ".lock";
    return endsWith(lowerAscii(file_name), suffix) &&
           isLiveRecoveryFile(file_name.substr(0, file_name.size() - suffix.size()));
}

bool ALDirectorQol::backupIsDuplicate(S64 source_size, S64 candidate_size, bool contents_equal)
{
    return source_size >= 0 && source_size == candidate_size && contents_equal;
}

bool ALDirectorQol::isLegacyLiveRecoveryName(const std::string& file_name)
{
    return lowerAscii(file_name) == std::string(LIVE_PREFIX) + ".xml";
}

bool ALDirectorQol::isLiveRecoveryFile(const std::string& file_name)
{
    return isLegacyLiveRecoveryName(file_name) || liveRecoveryPid(file_name) > 0;
}

bool ALDirectorQol::isPreviousSessionLiveFile(const std::string& file_name, S32 my_pid, bool own_file_written)
{
    if (isLegacyLiveRecoveryName(file_name))
        return true;
    const S32 pid = liveRecoveryPid(file_name);
    if (pid <= 0)
        return false;
    return pid != my_pid || !own_file_written;
}

bool ALDirectorQol::sceneSizeAllowed(size_t bytes)
{
    return bytes <= kDirectorSceneMaxBytes;
}

bool ALDirectorQol::isLegacyKeptBackupName(const std::string& file_name)
{
    const std::string name = lowerAscii(file_name);
    const std::string older = LIVE_KEPT_OLDER_TAG;
    return name == LIVE_KEPT_NAME ||
           (name.size() > older.size() && name.compare(0, older.size(), older) == 0);
}

std::string ALDirectorQol::crashBackupFileName(const BackupStamp& stamp, S32 counter, S32 pid)
{
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "scene-%04d%02d%02d-%02d%02d%02d", stamp.year, stamp.month,
             stamp.day, stamp.hour, stamp.minute, stamp.second);
    std::string name = buffer;
    if (pid > 0)
        name += "-p" + std::to_string(pid);
    if (counter > 0)
        name += "-" + std::to_string(counter);
    return name + ".xml";
}

std::string ALDirectorQol::uniqueCrashBackupName(const BackupStamp& stamp,
                                                 const std::vector<std::string>& existing, S32 pid)
{
    for (S32 counter = 0;; ++counter)
    {
        const std::string candidate = crashBackupFileName(stamp, counter, pid);
        if (!containsInsensitive(existing, candidate))
            return candidate;
    }
}

ALDirectorQol::ParsedBackupName ALDirectorQol::parseCrashBackupName(const std::string& file_name)
{
    ParsedBackupName parsed;
    const std::string name = lowerAscii(file_name);
    // scene-YYYYMMDD-HHMMSS[-N].xml : "scene-" 6, date 8, '-' at 14, time 6, then the tail.
    constexpr size_t TAIL = 21;
    if (name.size() < TAIL + 4 || name.compare(0, 6, "scene-") != 0 || !endsWith(name, ".xml") ||
        !allDigits(name, 6, 8) || name[14] != '-' || !allDigits(name, 15, 6))
        return parsed;
    const size_t end = name.size() - 4;
    size_t at = TAIL;
    if (at < end && name.compare(at, 2, "-p") == 0)
    {
        // "-p<pid>": one to ten digits.
        size_t digits = 0;
        while (at + 2 + digits < end && name[at + 2 + digits] >= '0' && name[at + 2 + digits] <= '9')
            ++digits;
        if (digits == 0 || digits > 10)
            return parsed;
        at += 2 + digits;
    }
    if (at != end)
    {
        // "-N": one to six digits, no leading zero, at least 1.
        const size_t count = end - at - 1;
        if (name[at] != '-' || end < at + 2 || count > 6 || !allDigits(name, at + 1, count) ||
            name[at + 1] == '0')
            return parsed;
        parsed.counter = std::atoi(name.substr(at + 1, count).c_str());
    }
    const S32 month = std::atoi(name.substr(10, 2).c_str());
    const S32 day = std::atoi(name.substr(12, 2).c_str());
    const S32 hour = std::atoi(name.substr(15, 2).c_str());
    const S32 minute = std::atoi(name.substr(17, 2).c_str());
    const S32 second = std::atoi(name.substr(19, 2).c_str());
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60)
        return ParsedBackupName();
    parsed.stamp = (U64)std::strtoull((name.substr(6, 8) + name.substr(15, 6)).c_str(), nullptr, 10);
    parsed.valid = true;
    return parsed;
}

bool ALDirectorQol::crashBackupNewer(const std::string& left, const std::string& right)
{
    const ParsedBackupName a = parseCrashBackupName(left);
    const ParsedBackupName b = parseCrashBackupName(right);
    if (a.valid != b.valid)
        return a.valid; // a name that does not parse is the oldest
    if (a.valid)
    {
        if (a.stamp != b.stamp)
            return a.stamp > b.stamp;
        if (a.counter != b.counter)
            return a.counter > b.counter;
    }
    return lowerAscii(left) > lowerAscii(right); // deterministic tie-break
}

std::vector<std::string> ALDirectorQol::sortCrashBackupsNewestFirst(std::vector<std::string> names)
{
    std::sort(names.begin(), names.end(), &ALDirectorQol::crashBackupNewer);
    return names;
}

std::vector<std::string> ALDirectorQol::crashBackupsToPrune(
    std::vector<std::string> names, size_t keep, const std::vector<std::string>& protected_names)
{
    names = sortCrashBackupsNewestFirst(std::move(names));
    std::vector<std::string> doomed;
    // names is newest first, so walk the excess from its end: the OLDEST go first.
    for (size_t index = names.size(); index > keep; --index)
        if (!containsInsensitive(protected_names, names[index - 1]))
            doomed.push_back(names[index - 1]);
    return doomed;
}

std::string ALDirectorQol::uniqueCorruptName(const std::string& backup_name,
                                             const std::vector<std::string>& existing)
{
    for (S32 counter = 0;; ++counter)
    {
        const std::string candidate = counter == 0
            ? backup_name + ".corrupt"
            : backup_name + "." + std::to_string(counter) + ".corrupt";
        if (!containsInsensitive(existing, candidate))
            return candidate;
    }
}

std::vector<std::string> ALDirectorQol::corruptBackupsToPrune(std::vector<std::string> names,
                                                              size_t keep)
{
    // Order by the backup the file came from ("<backup>.xml[.N].corrupt").
    const auto source = [](const std::string& name)
    {
        const size_t at = lowerAscii(name).find(".xml");
        return at == std::string::npos ? name : name.substr(0, at + 4);
    };
    std::sort(names.begin(), names.end(), [&source](const std::string& left, const std::string& right)
    {
        const std::string a = source(left), b = source(right);
        if (a != b)
            return ALDirectorQol::crashBackupNewer(a, b);
        return lowerAscii(left) > lowerAscii(right);
    });
    std::vector<std::string> doomed;
    for (size_t index = names.size(); index > keep; --index) // oldest first
        doomed.push_back(names[index - 1]);
    return doomed;
}

ALDirectorQol::LoginAction ALDirectorQol::loginAction(bool have_new_backup, S32 mode)
{
    if (!have_new_backup)
        return LoginAction::NONE;
    if (mode == 1)
        return LoginAction::RESTORE_AUTO;
    if (mode == 2)
        return LoginAction::KEEP;
    return LoginAction::OFFER;
}

ALDirectorQol::PromptAction ALDirectorQol::promptAction(S32 option, bool remember, bool replayed)
{
    // A replayed answer was not given by the user now: it never restores or deletes.
    if (replayed)
        return PromptAction::KEEP;
    if (option == 0)
        return PromptAction::RESTORE;
    // "Don't ask me again" + Discard only sets the mode; it never deletes this backup.
    if (option == 1)
        return remember ? PromptAction::KEEP : PromptAction::DISCARD;
    return PromptAction::KEEP;
}

S32 ALDirectorQol::rememberedRecoveryMode(S32 option, bool remember, bool replayed)
{
    if (!remember || replayed)
        return -1;
    if (option == 0)
        return 1;
    if (option == 1)
        return 2;
    return -1;
}

bool ALDirectorQol::mayClearPromptIgnore(bool replayed)
{
    return !replayed;
}

std::string ALDirectorQol::backupFileKey(const std::string& file_name, S64 size, S64 mtime)
{
    return lowerAscii(file_name) + "|" + std::to_string(size) + "|" + std::to_string(mtime);
}

bool ALDirectorQol::isReservedSceneName(const std::string& name)
{
    std::string trimmed = lowerAscii(name);
    const size_t first = trimmed.find_first_not_of(' ');
    if (first == std::string::npos)
        return false;
    trimmed.erase(0, first);
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '.'))
        trimmed.pop_back();
    // Hidden / recovery files (".director_recovery", "_2", "_kept") start with '.'.
    if (trimmed.empty() || trimmed[0] == '.')
        return true;
    return trimmed == "crash_backups";
}

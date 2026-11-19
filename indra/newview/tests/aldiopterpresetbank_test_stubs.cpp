/**
 * @file aldiopterpresetbank_test_stubs.cpp
 * @brief Minimal link-time stub so aldiopterpresetbank_test.cpp does not have
 * to drag in the full LLNotifications subsystem.
 *
 * aldiopterpresetbank.cpp (production code under test) references:
 *   - gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, filename) -- only
 *     on the (untested-by-default) non-test-dir path, but the symbol must
 *     still resolve at link time since the call is behind a runtime branch,
 *     not a compile-time one. This test target links `llfilesystem`
 *     (CMakeLists.txt test_libs), which already compiles lldir.cpp /
 *     lldir_win32.cpp and defines the REAL LLDir/gDirUtilp -- so this file
 *     must NOT also define them (Codex review finding F1: doing so collided
 *     with the real symbols and produced LNK2005). The test always calls
 *     ALDiopterPresetBankTest::setTestDir() first, so the real gDirUtilp
 *     path is never actually exercised at runtime; it only needs to link.
 *   - LLNotificationsUtil::add(name) on the (rare) rename-to-.stale failure
 *     path. The real implementation reaches the full LLNotifications
 *     singleton and template-loading machinery, which is far too heavy for
 *     a TUT unit test; this stub just returns a null LLNotificationPtr.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llnotificationsutil.h"

namespace LLNotificationsUtil
{
    LLNotificationPtr add(const std::string& /*name*/)
    {
        return LLNotificationPtr();
    }
}

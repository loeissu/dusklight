#pragma once

#include <borealis/app_info.hpp>

namespace dusk {
    /** Application identity fields for Borealis modules */
    inline constexpr borealis::AppInfo AppInfo{
        .orgName = "TwilitRealm",
        .appName = "Dusklight",
        // 汉化版：更新检查指向本 fork 的 Releases，由 CI 自动发布三平台汉化包
        .githubOwner = "loeissu",
        .githubRepo = "dusklight",
        .discordApplicationId = "1495632471994405035",
    };

    /**
     * \brief The internal application name for the game.
     *
     * This gets used for file paths and such, and cannot be changed!
     */
    constexpr auto AppName = "Dusklight";

}

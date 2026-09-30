#include "menu_bar.hpp"

#include "achievements.hpp"
#include "editor.hpp"
#include "mod_updates.hpp"
#include "modal.hpp"
#include "mods_window.hpp"
#include "prelaunch.hpp"
#include "settings.hpp"
#include "ui.hpp"
#include "warp.hpp"
#include "window.hpp"

#include "dusk/game_mode.hpp"
#include "dusk/livesplit.h"
#include "dusk/main.h"
#include "dusk/mods/svc/ui.hpp"
#include "dusk/settings.h"
#include "dusk/speedrun.h"

#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_audio.h"

#include <aurora/rmlui.hpp>
#include <imgui.h>
#include <RmlUi/Core.h>

#include <cmath>

namespace dusk::ui {
namespace {

const Rml::String kDocumentSource = R"RML(
<rml>
<head>
    <link type="text/rcss" href="res/rml/theme.rcss" />
    <link type="text/rcss" href="res/rml/mod_common.rcss" />
    <link type="text/rcss" href="res/rml/tabbing.rcss" />
    <link type="text/rcss" href="res/rml/popup.rcss" />
</head>
<body>
    <popup id="popup" />
</body>
</rml>
)RML";
}

MenuBar::MenuBar()
    : Document(kDocumentSource, false, DocumentScope::MenuBar),
      mRoot(mDocument->GetElementById("popup")) {
    mTabBar = std::make_unique<TabBar>(mRoot, TabBar::Props{
                                                  .onClose =
                                                      [this] {
                                                          mDoAud_seStartMenu(kSoundMenuClose);
                                                          hide(false);
                                                      },
                                                  .autoSelect = false,
                                              });

    // Hide document after transition completion
    listen(mRoot, Rml::EventId::Transitionend, [this](Rml::Event& event) {
        if (event.GetTargetElement() == mRoot && !mRoot->HasAttribute("open") &&
            Document::visible())
        {
            Document::hide(mPendingClose);
        }
    });

    build_tabs();
}

void MenuBar::build_tabs() {
    mTabBar->add_tab("设置", [this] { push(std::make_unique<SettingsWindow>()); });

    if (getSettings().backend.enableAdvancedSettings) {
        mTabBar->add_tab("传送", [this] { push(std::make_unique<WarpWindow>()); });
        mTabBar->add_tab("编辑器", [this] { push(std::make_unique<EditorWindow>()); });
    }

    // Only allow us to access achievements if we are playing on a game mode that uses them
    if (gamemode::getGameModeManager().isCurrentGameMode(gamemode::kVanillaGameModeId) ||
        gamemode::getGameModeManager().isCurrentGameMode(speedrun::kSpeedrunGameModeId))
    {
        mTabBar->add_tab("成就", [this] { push(std::make_unique<AchievementsWindow>()); });
    }
    mModsButton = &mTabBar->add_tab("模组", [this] { push(std::make_unique<ModsWindow>()); });
    for (auto& tab : mods::svc::ui_mod_menu_tabs()) {
        mTabBar->add_tab(tab.label, std::move(tab.onSelected));
    }

    mTabBar->add_tab("重置", [this] {
        mTabBar->set_active_tab(-1);
        const auto dismiss = [](Modal& modal) { modal.pop(); };
        push(std::make_unique<Modal>(Modal::Props{
            .title = "重置游戏",
            .bodyRml = "未保存的进度将会丢失。<br/>"
                       "<modal-tip>提示：也可按住 Start+X+B 重置</modal-tip>",
            .actions =
                {
                    ModalAction{
                        .label = "取消",
                        .onPressed =
                            [this, dismiss](Modal& modal) {
                                mDoAud_seStartMenu(kSoundWindowClose);
                                dismiss(modal);
                            },
                    },
                    ModalAction{
                        .label = "重置",
                        .onPressed =
                            [this, dismiss](Modal& modal) {
                                mDoAud_seStartMenu(kSoundClick);
                                if (fpcM_SearchByName(fpcNm_LOGO_SCENE_e)) {
                                    dismiss(modal);
                                    return;
                                }
                                dismiss(modal);
                                if (gamemode::getGameModeManager().getRegisteredGameModes().size() >
                                    1) {
                                    // If game modes are registered, return to prelaunch on reset.
                                    prelaunch_state().returnToPrelaunchOnReset = true;
                                }
                                hide(false);
                                JUTGamePad::C3ButtonReset::sResetSwitchPushing = true;
                            },
                    },
                },
            .onDismiss = dismiss,
            .icon = "question-mark",
        }));
    });
    mTabBar->add_tab("退出", [this] {
        mTabBar->set_active_tab(-1);
        const auto dismiss = [](Modal& modal) { modal.pop(); };
        push(std::make_unique<Modal>(Modal::Props{
            .title = "退出 Dusklight",
            .bodyText = "未保存的进度将会丢失。",
            .actions =
                {
                    ModalAction{
                        .label = "取消",
                        .onPressed =
                            [dismiss](Modal& modal) {
                                mDoAud_seStartMenu(kSoundWindowClose);
                                dismiss(modal);
                            },
                    },
                    ModalAction{
                        .label = "退出",
                        .onPressed =
                            [dismiss](Modal& modal) {
                                mDoAud_seStartMenu(kSoundClick);
                                dismiss(modal);
                                IsRunning = false;
                            },
                    },
                },
            .onDismiss = dismiss,
            .icon = "question-mark",
        }));
    });

    if (speedrun::isActive()) {
        mTabBar->add_tab("重置速通", [this] {
            mTabBar->set_active_tab(-1);
            mDoAud_seStartMenu(kSoundClick);
            speedrun::g_speedrunInfo.reset();
            speedrun::reset();
            JUTGamePad::C3ButtonReset::sResetSwitchPushing = true;
            hide(false);
        });
    }
}

void MenuBar::show() {
    Document::show();
    mRoot->SetAttribute("open", "");
    mTabBar->set_active_tab(-1);
    if (!mTabBar->focus_tab(mFocusedTabTitle)) {
        mTabBar->focus();
    }
}

void MenuBar::hide(bool close) {
    mFocusedTabTitle = mTabBar->focused_tab_title();
    mRoot->RemoveAttribute("open");
    if (close) {
        mPendingClose = true;
    }
}

void MenuBar::update() {
    if (mModsButton) {
        set_mod_update_badge(*mModsButton);
    }
    update_safe_area();
    Document::update();
}

void MenuBar::update_safe_area() noexcept {
    if (mDocument == nullptr || mTabBar == nullptr) {
        return;
    }

    // Avoid ImGui menu bar if shown
    if (const auto* viewport = ImGui::GetMainViewport();
        viewport != nullptr && mTopMargin != viewport->WorkPos.y)
    {
        mTopMargin = viewport->WorkPos.y;
        mRoot->SetProperty(Rml::PropertyId::MarginTop, Rml::Property(mTopMargin, Rml::Unit::DP));
    }

    Rml::Context* context = mDocument->GetContext();
    Insets safeInsets = safe_area_insets(context);
    safeInsets = {
        0.0f,
        std::round(safeInsets.right),
        0.0f,
        std::round(safeInsets.left),
    };
    if (safeInsets == mTabBarPadding) {
        return;
    }

    mTabBarPadding = safeInsets;
    auto* tabBar = mTabBar->root();
    tabBar->SetProperty(
        Rml::PropertyId::PaddingRight, Rml::Property(safeInsets.right, Rml::Unit::PX));
    tabBar->SetProperty(
        Rml::PropertyId::PaddingLeft, Rml::Property(safeInsets.left, Rml::Unit::PX));
    if (auto* close = tabBar->QuerySelector("close")) {
        close->SetProperty(Rml::PropertyId::Right,
            Rml::Property(safeInsets.right + 8.0f * context->GetDensityIndependentPixelRatio(),
                Rml::Unit::PX));
    }
}

bool MenuBar::visible() const {
    return mRoot->HasAttribute("open");
}

bool MenuBar::handle_nav_command(Rml::Event& event, NavCommand cmd) {
    if (!getSettings().backend.wasPresetChosen) {
        return true;
    }
    if (cmd == NavCommand::Cancel && visible()) {
        mDoAud_seStartMenu(kSoundMenuClose);
        hide(false);
        return true;
    }
    return Document::handle_nav_command(event, cmd);
}

bool MenuBar::focus() {
    return mTabBar->focus();
}

void MenuBar::refresh_tabs() {
    auto* menuBar = static_cast<MenuBar*>(find_document(DocumentScope::MenuBar));
    if (menuBar == nullptr) {
        return;
    }
    const auto focusedTitle = menuBar->mTabBar->focused_tab_title();
    if (!focusedTitle.empty()) {
        menuBar->mFocusedTabTitle = focusedTitle;
    }
    menuBar->mTabBar->clear_tabs();
    menuBar->build_tabs();
    if (menuBar->visible() && !menuBar->mTabBar->focus_tab(menuBar->mFocusedTabTitle)) {
        menuBar->mTabBar->focus();
    }
}

}  // namespace dusk::ui

#include "menu_bar.hpp"

#include <RmlUi/Core.h>

#include "Z2AudioLib/Z2SeMgr.h"
#include "m_Do/m_Do_audio.h"

#include "achievements.hpp"
#include "aurora/rmlui.hpp"
#include "dusk/livesplit.h"
#include "dusk/main.h"
#include "dusk/mods/svc/ui.hpp"
#include "dusk/settings.h"
#include "dusk/speedrun.h"
#include "editor.hpp"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "imgui.h"
#include "modal.hpp"
#include "mods_window.hpp"
#include "settings.hpp"
#include "ui.hpp"
#include "warp.hpp"
#include "window.hpp"

#include <chrono>
#include <cmath>

namespace dusk::ui {
namespace {

const Rml::String kDocumentSource = R"RML(
<rml>
<head>
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
    mTabBar->add_tab("设置", [this] { push(std::make_unique<SettingsWindow>()); });

    if (getSettings().backend.enableAdvancedSettings) {
        mTabBar->add_tab("传送", [this] { push(std::make_unique<WarpWindow>()); });
        mTabBar->add_tab("编辑器", [this] { push(std::make_unique<EditorWindow>()); });
    }

    mTabBar->add_tab("成就", [this] { push(std::make_unique<AchievementsWindow>()); });
    mTabBar->add_tab("模组", [this] { push(std::make_unique<ModsWindow>()); });
    for (auto& tab : mods::svc::ui_mod_menu_tabs()) {
        mTabBar->add_tab(tab.label, std::move(tab.onSelected));
    }

    mTabBar->add_tab("重置", [this] {
        mTabBar->set_active_tab(-1);
        const auto dismiss = [](Modal& modal) { modal.pop(); };
        push(std::make_unique<Modal>(Modal::Props{
            .title = "重置游戏",
            .bodyRml = "未保存的进度将丢失。<br/>"
                       "<span class=\"tip\">提示：也可以按住 Start+X+B 重置</span>",
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
                                JUTGamePad::C3ButtonReset::sResetSwitchPushing = true;
                                dismiss(modal);
                                hide(false);
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
            .bodyRml = "未保存的进度将丢失。",
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

    if (getSettings().game.speedrunMode) {
        mTabBar->add_tab("重置计时器", [this] {
            mTabBar->set_active_tab(-1);
            mDoAud_seStartMenu(kSoundClick);
            m_speedrunInfo.reset();
            if (getSettings().game.liveSplitEnabled) {
                dusk::speedrun::reset();
            }
            hide(false);
        });
    }

    // Hide document after transition completion
    listen(mRoot, Rml::EventId::Transitionend, [this](Rml::Event& event) {
        if (event.GetTargetElement() == mRoot && !mRoot->HasAttribute("open") &&
            Document::visible())
        {
            Document::hide(mPendingClose);
        }
    });
}

void MenuBar::show() {
    Document::show();
    mRoot->SetAttribute("open", "");
    mTabBar->set_active_tab(-1);
    if (!mTabBar->focus_tab(mFocusedTabIndex)) {
        mTabBar->focus();
    }
}

void MenuBar::hide(bool close) {
    mFocusedTabIndex = mTabBar->focused_tab_index();
    mRoot->RemoveAttribute("open");
    if (close) {
        mPendingClose = true;
    }
}

void MenuBar::update() {
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

void MenuBar::rebuild() {
    for (auto& doc : get_document_stack()) {
        if (auto* menuBar = dynamic_cast<MenuBar*>(doc.get())) {
            const bool wasVisible = menuBar->visible();
            auto next = std::make_unique<MenuBar>();
            next->mFocusedTabIndex = menuBar->mFocusedTabIndex;
            next->mWasVisible = menuBar->mWasVisible;
            doc = std::move(next);
            if (wasVisible) {
                doc->show();
            }
            break;
        }
    }
}

}  // namespace dusk::ui

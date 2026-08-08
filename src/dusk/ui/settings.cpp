#include "settings.hpp"

#include "aurora/gfx.h"
#include "bool_button.hpp"
#include "controller_config.hpp"
#include "dusk/app_info.hpp"
#include "dusk/audio/DuskAudioSystem.h"
#include "dusk/audio/DuskDsp.hpp"
#include "dusk/config.hpp"
#include "dusk/hotkeys.h"
#include "dusk/data.hpp"
#include "dusk/imgui/ImGuiEngine.hpp"
#include "dusk/io.hpp"
#include "dusk/presentation.hpp"
#include <borealis/io.hpp>
#include <borealis/file_select.hpp>
#include "dusk/livesplit.h"
#include "dusk/discord_presence.hpp"
#include "dusk/speedrun.h"
#include "graphics_tuner.hpp"
#include "m_Do/m_Do_main.h"
#include "menu_bar.hpp"
#include "modal.hpp"
#include "number_button.hpp"
#include "menu_bar.hpp"
#include "pane.hpp"
#include "prelaunch.hpp"
#include "touch_controls_editor.hpp"
#include "ui.hpp"

#include <aurora/lib/window.hpp>
#include <SDL3/SDL_filesystem.h>
#include <fmt/format.h>

#if BOREALIS_HAS_SENTRY
#include <borealis/sentry.hpp>
#endif

#include <algorithm>
#include <filesystem>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(TARGET_ANDROID) || defined(__ANDROID__) || \
    (defined(__APPLE__) && TARGET_OS_IOS && !TARGET_OS_MACCATALYST)
#define TOUCH_CONTROLS_AVAILABLE true
#else
#define TOUCH_CONTROLS_AVAILABLE false
#endif

namespace dusk::ui {
namespace {

constexpr std::array kLanguageNames = {
    "英语",
    "德语",
    "法语",
    "西班牙语",
    "意大利语",
};

constexpr std::array kCardFileTypes = {
    "记忆卡镜像",
    "GCI 文件夹",
};

constexpr std::array kFpsOverlayCornerNames = {
    "左上角",
    "右上角",
    "左下角",
    "右下角",
};

constexpr std::array kInterpolationModes = {
    "关闭",
    "锁定",
    "无限制",
};

constexpr std::array kTouchTargetingLabels = {
    "混合",
    "按住",
    "切换",
};

constexpr std::array kTouchTargetingDescriptions = {
    "找到目标时轻点一次锁定；未找到目标时双击以按住 L。",
    "手指按住按键期间 L 保持按住。",
    "轻点 L 保持按住，再次轻点松开。",
};

constexpr std::array kGyroInputModeLabels = {
    "传感器",
    "鼠标",
};

constexpr std::array kMenuScalingModeLabels = {
    "GameCube",
    "Wii",
    "Dusklight",
};

constexpr std::array kMagicArmorModes = {
    "普通",
    "受击消耗",
    "双倍防御",
    "无敌",
    "仅外观",
};

bool try_parse_backend(std::string_view backend, AuroraBackend& outBackend) {
    if (backend == "auto") {
        outBackend = BACKEND_AUTO;
        return true;
    }
    if (backend == "d3d11") {
        outBackend = BACKEND_D3D11;
        return true;
    }
    if (backend == "d3d12") {
        outBackend = BACKEND_D3D12;
        return true;
    }
    if (backend == "metal") {
        outBackend = BACKEND_METAL;
        return true;
    }
    if (backend == "vulkan") {
        outBackend = BACKEND_VULKAN;
        return true;
    }
    if (backend == "opengl") {
        outBackend = BACKEND_OPENGL;
        return true;
    }
    if (backend == "opengles") {
        outBackend = BACKEND_OPENGLES;
        return true;
    }
    if (backend == "webgpu") {
        outBackend = BACKEND_WEBGPU;
        return true;
    }
    if (backend == "null") {
        outBackend = BACKEND_NULL;
        return true;
    }

    return false;
}

std::string_view backend_name(AuroraBackend backend) {
    switch (backend) {
    default:
        return "Auto";
    case BACKEND_D3D12:
        return "D3D12";
    case BACKEND_D3D11:
        return "D3D11";
    case BACKEND_METAL:
        return "Metal";
    case BACKEND_VULKAN:
        return "Vulkan";
    case BACKEND_OPENGL:
        return "OpenGL";
    case BACKEND_OPENGLES:
        return "OpenGL ES";
    case BACKEND_WEBGPU:
        return "WebGPU";
    case BACKEND_NULL:
        return "Null";
    }
}

std::string_view backend_id(AuroraBackend backend) {
    switch (backend) {
    default:
        return "auto";
    case BACKEND_D3D12:
        return "d3d12";
    case BACKEND_D3D11:
        return "d3d11";
    case BACKEND_METAL:
        return "metal";
    case BACKEND_VULKAN:
        return "vulkan";
    case BACKEND_OPENGL:
        return "opengl";
    case BACKEND_OPENGLES:
        return "opengles";
    case BACKEND_WEBGPU:
        return "webgpu";
    case BACKEND_NULL:
        return "null";
    }
}

std::vector<AuroraBackend> available_backends() {
    std::vector<AuroraBackend> backends;
    backends.emplace_back(BACKEND_AUTO);
    size_t backendCount = 0;
    const AuroraBackend* raw = aurora_get_available_backends(&backendCount);
    for (size_t i = 0; i < backendCount; ++i) {
        // Do not expose NULL
        if (raw[i] != BACKEND_NULL) {
            backends.emplace_back(raw[i]);
        }
    }
    return backends;
}

AuroraBackend configured_backend() {
    AuroraBackend configuredBackend = BACKEND_AUTO;
    const auto configuredId = getSettings().backend.graphicsBackend.getValue();
    if (!try_parse_backend(configuredId, configuredBackend)) {
        configuredBackend = BACKEND_AUTO;
    }
    return configuredBackend;
}

Rml::String configured_data_path_display_name() {
    const auto path = data::abbreviated_path_string(data::configured_data_path());
    if (path.empty()) {
        return "(none)";
    }

    auto display = borealis::file_select::display_name(path);
    if (display.empty()) {
        return path;
    }
    return display;
}

class DataFolderPathText : public Component {
public:
    explicit DataFolderPathText(Rml::Element* parent) : Component(append(parent, "div")) {}

    void update() override {
        const Rml::String rml =
            "<span class=\"data-folder-current\">当前数据文件夹：<br/>" +
            escape(data::abbreviated_path_string(data::configured_data_path())) + "</span>";
        if (rml != mCurrentRml) {
            mRoot->SetInnerRML(rml);
            mCurrentRml = rml;
        }
        Component::update();
    }

private:
    Rml::String mCurrentRml;
};

void show_data_folder_error_modal(std::string_view message) {
    auto dismiss = [](Modal& modal) {
        mDoAud_seStartMenu(kSoundWindowClose);
        modal.pop();
    };
    push_document(std::make_unique<Modal>(Modal::Props{
        .title = "数据文件夹未更改",
        .bodyRml = escape(message),
        .actions =
            {
                ModalAction{
                    .label = "确定",
                    .onPressed = dismiss,
                },
            },
        .onDismiss = dismiss,
        .icon = "warning",
    }));
    if (auto* doc = top_document()) {
        doc->focus();
    }
}

void data_folder_dialog_callback(borealis::file_select::Result result) {
    if (result.status == borealis::file_select::Status::Canceled) {
        return;
    }
    if (result.status != borealis::file_select::Status::Selected || result.locations.empty()) {
        show_data_folder_error_modal("无法打开文件夹选择器。");
        return;
    }

    std::string dataPathError;
    if (data::set_custom_data_path(result.locations.front(), &dataPathError)) {
        mDoAud_seStartMenu(kSoundItemChange);
        return;
    }

    if (dataPathError.empty()) {
        dataPathError =
            fmt::format("{} 无法使用所选文件夹作为其数据文件夹。", AppName);
    }
    show_data_folder_error_modal(dataPathError);
}

const Rml::String kInternalResolutionHelpText =
    "配置游戏渲染分辨率。数值越高对显卡要求越高。";
const Rml::String kShadowResolutionHelpText =
    "配置阴影贴图分辨率。数值越高阴影质量越好，但会占用更多 GPU 和内存。";
const Rml::String kResamplerHelpText =
    "配置内部分辨率缩放至最终画面时使用的采样方式。";
const Rml::String kBloomHelpText =
    "配置后期处理泛光效果。经典模式使用原版泛光，Dusklight 模式使用更高质量的泛光。";
const Rml::String kBloomBrightnessHelpText =
    "配置泛光强度。数值越高，明亮区域的光晕越强。";
const Rml::String kDepthOfFieldHelpText =
    "配置后期处理景深效果。经典模式使用原版景深，Dusklight 模式使用更高质量的景深。";
const Rml::String kUnlockFramerateHelpText =
    "<br/>通过帧间插值实现更高的帧率。<br/><br/>可能引入轻微画面瑕疵或动画异常。";
const Rml::String kTextureReplacementHelpText =
    "启用已安装的贴图替换。";

int float_setting_percent(ConfigVar<float>& var) {
    return static_cast<int>(var.getValue() * 100.0f + 0.5f);
}

bool gyro_enabled() {
    return getSettings().game.enableGyroAim || getSettings().game.enableGyroRollgoal;
}

Rml::String touch_targeting_label(TouchTargeting targeting) {
    const auto index = static_cast<std::size_t>(targeting);
    if (index >= kTouchTargetingLabels.size()) {
        return "未知";
    }
    return kTouchTargetingLabels[index];
}

struct ConfigBoolProps {
    Rml::String key;
    Rml::String icon;
    Rml::String helpText;
    std::function<void(bool)> onChange;
    std::function<bool()> isDisabled;
};

SelectButton& config_bool_select(
    Pane& leftPane, Pane& rightPane, ConfigVar<bool>& var, ConfigBoolProps props) {
    auto& button = leftPane.add_child<BoolButton>(BoolButton::Props{
        .key = std::move(props.key),
        .icon = std::move(props.icon),
        .getValue = [&var] { return var.getValue(); },
        .setValue =
            [&var, callback = std::move(props.onChange)](bool value) {
                if (value == var.getValue()) {
                    return;
                }
                var.setValue(value);
                config::save();
                if (callback) {
                    callback(value);
                }
            },
        .isDisabled = std::move(props.isDisabled),
        .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
    });
    leftPane.register_control(
        button, rightPane, [helpText = std::move(props.helpText)](Pane& pane) {
            pane.clear();
            pane.add_rml(helpText);
        });
    return button;
}

void add_speedrun_disabled_option(Pane& leftPane, Pane& rightPane, ConfigVar<bool>& var,
    const Rml::String& key, const Rml::String& helpText) {
    config_bool_select(leftPane, rightPane, var, {
        .key = key,
        .helpText = helpText,
        .isDisabled = [] { return getSettings().game.speedrunMode.getValue(); },
    });
}

SelectButton& config_percent_select(Pane& leftPane, Pane& rightPane, ConfigVar<float>& var,
    Rml::String key, Rml::String helpText, int min, int max, int step = 5,
    std::function<bool()> isDisabled = {}) {
    auto& button = leftPane.add_child<NumberButton>(NumberButton::Props{
        .key = std::move(key),
        .getValue = [&var] { return float_setting_percent(var); },
        .setValue =
            [&var, min, max](int value) {
                var.setValue(std::clamp(value, min, max) / 100.0f);
                config::save();
            },
        .isDisabled = std::move(isDisabled),
        .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
        .min = min,
        .max = max,
        .step = step,
        .suffix = "%",
    });
    leftPane.register_control(button, rightPane, [helpText = std::move(helpText)](Pane& pane) {
        pane.clear();
        pane.add_rml(helpText);
    });
    return button;
}

SelectButton& config_int_select(Pane& leftPane, Pane& rightPane, ConfigVar<int>& var,
    Rml::String key, Rml::String helpText, int min, int max, int step = 5,
    std::function<bool()> isDisabled = {}, std::function<void(int)> onChange = {},
    std::string suffix = "") {
    auto& button = leftPane.add_child<NumberButton>(NumberButton::Props{
        .key = std::move(key),
        .getValue = [&var] { return var.getValue(); },
        .setValue =
            [&var, min, max, callback = std::move(onChange)](int value) {
                const int clampedValue = std::clamp(value, min, max);
                var.setValue(clampedValue);
                config::save();
                if (callback) {
                    callback(clampedValue);
                }
            },
        .isDisabled = std::move(isDisabled),
        .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
        .min = min,
        .max = max,
        .step = step,
        .suffix = suffix,
    });
    leftPane.register_control(button, rightPane, [helpText = std::move(helpText)](Pane& pane) {
        pane.clear();
        pane.add_text(helpText);
    });
    return button;
}

template <typename T>
void graphics_tuner_control(Window& window, Pane& leftPane, Pane& rightPane, ConfigVar<T>& var,
    const GraphicsTunerProps& props) {
    leftPane.register_control(
        leftPane
            .add_select_button({
                .key = props.title,
                .getValue =
                    [&var, option = props.option] {
                        if constexpr (std::is_same_v<T, float>) {
                            return format_graphics_setting_value(
                                option, float_setting_percent(var));
                        } else {
                            return format_graphics_setting_value(
                                option, static_cast<int>(var.getValue()));
                        }
                    },
                .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
                .submit = false,
            })
            .on_nav_command([&window, props](Rml::Event&, NavCommand cmd) {
                if (cmd == NavCommand::Confirm || cmd == NavCommand::Left ||
                    cmd == NavCommand::Right) {
                    window.push(std::make_unique<GraphicsTuner>(props));
                    return true;
                }
                return false;
            }),
        rightPane, [helpText = props.helpText](Pane& pane) {
            pane.clear();
            pane.add_text(helpText);
        });
}

}  // namespace

SettingsWindow::SettingsWindow(bool prelaunch) : mPrelaunch(prelaunch) {
    if (prelaunch) {
        add_tab("启动", [this](Rml::Element* content) {
            auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
            auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

            leftPane.register_control(
                leftPane
                    .add_select_button({
                        .key = "游戏镜像",
                        .getValue =
                            [] {
                                const auto& path = prelaunch_state().configuredDiscPath;
                                std::string display;
                                if (path.empty()) {
                                    display = "（无）";
                                } else {
                                    display = borealis::file_select::display_name(path);
                                    if (display.empty()) {
                                        display = path;
                                    }
                                }
                                return display;
                            },
                        .isModified =
                            [] {
                                const auto& state = prelaunch_state();
                                const auto& active = state.activeDiscPath;
                                return !active.empty() && state.configuredDiscPath != active;
                            },
                    })
                    .on_pressed([] { open_iso_picker(); }),
                rightPane, [](Pane& pane) {
                    pane.add_rml(
                        "设置 Dusklight 启动游戏所用的镜像文件。<br/><br/>更改需要重启。");
                });
            if (data::manager().capabilities().canChangeLocation &&
                borealis::file_select::capabilities().canOpenFolder)
            {
                leftPane.register_control(
                    leftPane.add_select_button({
                        .key = "数据文件夹",
                        .getValue = [] { return configured_data_path_display_name(); },
                        .isModified = [] { return data::is_data_path_restart_pending(); },
                    }),
                    rightPane, [](Pane& pane) {
                        pane.add_text("数据文件夹用于存放 Dusklight 的设置、存档、日志、贴图替换等应用数据。");
                        pane.add_child<DataFolderPathText>();
#if DUSK_CAN_OPEN_DATA_FOLDER
                        pane.add_button("打开数据文件夹").on_pressed([] {
                            if (data::open_data_path()) {
                                mDoAud_seStartMenu(kSoundClick);
                            }
                        });
#endif
                        pane.add_button("更改数据文件夹").on_pressed([] {
                            const auto defaultLocation =
                                borealis::io::fs_path_to_string(data::configured_data_path());
                            borealis::file_select::open_folder(
                                {
                                    .parentWindow = aurora::window::get_sdl_window(),
                                    .defaultLocation = defaultLocation,
                                },
                                &data_folder_dialog_callback);
                        });
#if defined(_WIN32)
                        pane.add_button("便携模式").on_pressed([] {
                            if (data::set_portable_data_path()) {
                                mDoAud_seStartMenu(kSoundItemChange);
                            }
                        });
#endif
                        pane.add_button(
                                {
                                    .text = "恢复默认",
                                    .isDisabled = [] { return data::is_default_data_path(); },
                                })
                            .on_pressed([] {
                                if (data::reset_data_path()) {
                                    mDoAud_seStartMenu(kSoundItemChange);
                                }
                            });
                        pane.add_rml("数据将在重启后自动迁移。");
                    });
            }
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = "语言",
                    .getValue =
                        [] {
                            const auto& state = prelaunch_state();
                            if (!state.configuredDiscCanLaunch || state.configuredDiscInfo.region != iso::Region::Europe) {
                                return kLanguageNames[0];
                            }
                            const u8 idx = static_cast<u8>(getSettings().game.language.getValue());
                            return kLanguageNames[idx];
                        },
                    .isDisabled =
                        [] {
                            const auto& state = prelaunch_state();
                            return !state.configuredDiscCanLaunch ||
                                   state.configuredDiscInfo.region != iso::Region::Europe;
                        },
                    .isModified =
                        [] {
                            return getSettings().game.language.getValue() !=
                                   prelaunch_state().initialLanguage;
                        },
                }),
                rightPane, [](Pane& pane) {
                    for (int i = 0; i < kLanguageNames.size(); i++) {
                        pane.add_button({
                                            .text = kLanguageNames[i],
                                            .isSelected =
                                                [i] {
                                                    return getSettings().game.language.getValue() ==
                                                           static_cast<GameLanguage>(i);
                                                },
                                        })
                            .on_pressed([i] {
                                mDoAud_seStartMenu(kSoundItemChange);
                                getSettings().game.language.setValue(static_cast<GameLanguage>(i));
                                config::save();
                            });
                    }
                    pane.add_rml("<br/>更改需要重启。");
                });
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = "图形后端",
                    .getValue = [] { return Rml::String{backend_name(aurora_get_backend())}; },
                    .isModified =
                        [] {
                            return getSettings().backend.graphicsBackend.getValue() !=
                                   prelaunch_state().initialGraphicsBackend;
                        },
                }),
                rightPane, [](Pane& pane) {
                    const auto availableBackends = available_backends();
                    for (const auto backend : availableBackends) {
                        pane
                            .add_button({
                                .text = Rml::String{backend_name(backend)},
                                .isSelected = [backend] { return configured_backend() == backend; },
                            })
                            .on_pressed([backend] {
                                mDoAud_seStartMenu(kSoundItemChange);
                                getSettings().backend.graphicsBackend.setValue(
                                    std::string{backend_id(backend)});
                                config::save();
                            });
                    }
                    pane.add_rml("<br/>更改需要重启。");
                });
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = "存档文件类型",
                    .getValue =
                        [] {
                            return kCardFileTypes[getSettings().backend.cardFileType.getValue()];
                        },
                    .isModified =
                        [] {
                            return getSettings().backend.cardFileType.getValue() !=
                                   prelaunch_state().initialCardFileType;
                        },
                }),
                rightPane, [](Pane& pane) {
                    for (int i = 0; i < kCardFileTypes.size(); i++) {
                        pane
                            .add_button({
                                .text = kCardFileTypes[i],
                                .isSelected =
                                    [i] {
                                        return getSettings().backend.cardFileType.getValue() == i;
                                    },
                            })
                            .on_pressed([i] {
                                mDoAud_seStartMenu(kSoundItemChange);
                                getSettings().backend.cardFileType.setValue(i);
                                config::save();
                            });
                    }
                });
        });
    }

    add_tab("视频", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section("显示");

        leftPane.register_control(leftPane.add_button("切换全屏").on_pressed([] {
            mDoAud_seStartMenu(kSoundItemChange);
            getSettings().video.enableFullscreen.setValue(!getSettings().video.enableFullscreen);
            VISetWindowFullscreen(getSettings().video.enableFullscreen);
            config::save();
        }),
            rightPane, [](Pane& pane) { pane.clear(); });
        leftPane.register_control(leftPane.add_button("恢复默认窗口大小").on_pressed([] {
            mDoAud_seStartMenu(kSoundItemChange);
            getSettings().video.enableFullscreen.setValue(false);
            VISetWindowFullscreen(false);
            VISetWindowSize(FB_WIDTH * 2, FB_HEIGHT * 2);
            VICenterWindow();
        }),
            rightPane, [](Pane& pane) { pane.clear(); });
        config_bool_select(leftPane, rightPane, getSettings().video.enableVsync,
            {
                .key = "启用垂直同步",
                .helpText = "将帧率与显示器刷新率同步。",
                .onChange = [](bool value) { aurora_enable_vsync(value); },
            });
        config_bool_select(leftPane, rightPane, getSettings().video.lockAspectRatio,
            {
                .key = "锁定 4:3 宽高比",
                .helpText = "将游戏画面锁定为原始宽高比。",
                .onChange =
                    [](bool value) {
                        AuroraSetViewportPolicy(
                            value ? AURORA_VIEWPORT_FIT : AURORA_VIEWPORT_STRETCH);
                    },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.pauseOnFocusLost,
            {
                .key = "失去焦点时暂停",
                .helpText = "窗口失去焦点时暂停游戏。",
                .isDisabled = [] { return IsMobile || getSettings().game.speedrunMode; },
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "显示 FPS 计数器",
                .getValue =
                    [] {
                        if (!getSettings().video.enableFpsOverlay.getValue()) {
                            return Rml::String{"关闭"};
                        }
                        const int idx = getSettings().video.fpsOverlayCorner.getValue();
                        return Rml::String{kFpsOverlayCornerNames[idx]};
                    },
                .isModified =
                    [] {
                        const auto& enable = getSettings().video.enableFpsOverlay;
                        const auto& corner = getSettings().video.fpsOverlayCorner;
                        return enable.getValue() != enable.getDefaultValue() ||
                               (enable.getValue() && corner.getValue() != corner.getDefaultValue());
                    },
            }),
            rightPane, [](Pane& pane) {
                pane.add_button(
                        {
                            .text = "关闭",
                            .isSelected =
                                [] { return !getSettings().video.enableFpsOverlay.getValue(); },
                        })
                    .on_pressed([] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        getSettings().video.enableFpsOverlay.setValue(false);
                        config::save();
                    });
                for (int i = 0; i < static_cast<int>(kFpsOverlayCornerNames.size()); ++i) {
                    pane.add_button(
                            {
                                .text = kFpsOverlayCornerNames[i],
                                .isSelected =
                                    [i] {
                                        return getSettings().video.enableFpsOverlay.getValue() &&
                                               getSettings().video.fpsOverlayCorner.getValue() == i;
                                    },
                            })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().video.enableFpsOverlay.setValue(true);
                            getSettings().video.fpsOverlayCorner.setValue(i);
                            config::save();
                        });
                }
                pane.add_rml(
                    "<br/>在游戏过程中于屏幕角落显示当前帧率。");
            });
        config_bool_select(leftPane, rightPane, getSettings().video.rememberWindowSize,
            {
                .key = "记住窗口大小",
                .helpText = "打开 Dusklight 时保存并恢复上次会话的窗口大小。",
                .onChange =
                    [](bool value) {
                        if (value && !dusk::getSettings().video.enableFullscreen) {
                            const auto windowSize = aurora::window::get_window_size();
                            dusk::getSettings().video.lastWindowWidth.setValue(windowSize.width);
                            dusk::getSettings().video.lastWindowHeight.setValue(windowSize.height);
                            dusk::config::save();
                        }
                    },
                .isDisabled = [] { return IsMobile; },
            });
        leftPane.add_section("分辨率");
        graphics_tuner_control(*this, leftPane, rightPane,
            getSettings().game.internalResolutionScale,
            GraphicsTunerProps{
                .option = GraphicsOption::InternalResolution,
                .title = "内部分辨率",
                .helpText = kInternalResolutionHelpText,
                .valueMin = 0,
                .valueMax = 12,
                .defaultValue = 0,
            });
        graphics_tuner_control(*this, leftPane, rightPane,
            getSettings().game.shadowResolutionMultiplier,
            GraphicsTunerProps{
                .option = GraphicsOption::ShadowResolution,
                .title = "阴影分辨率",
                .helpText = kShadowResolutionHelpText,
                .valueMin = 1,
                .valueMax = 8,
                .defaultValue = 1,
            });
        graphics_tuner_control(*this, leftPane, rightPane, getSettings().game.resampler,
            GraphicsTunerProps{
                .option = GraphicsOption::Resampler,
                .title = "输出重采样",
                .helpText = kResamplerHelpText,
                .valueMin = static_cast<int>(Resampler::Bilinear),
                .valueMax = static_cast<int>(Resampler::Area),
                .defaultValue = static_cast<int>(Resampler::Bilinear),
            });

        leftPane.add_section("后期处理");
        graphics_tuner_control(*this, leftPane, rightPane, getSettings().game.bloomMode,
            GraphicsTunerProps{
                .option = GraphicsOption::BloomMode,
                .title = "泛光",
                .helpText = kBloomHelpText,
                .valueMin = static_cast<int>(BloomMode::Off),
                .valueMax = static_cast<int>(BloomMode::Dusk),
                .defaultValue = static_cast<int>(BloomMode::Classic),
            });
        graphics_tuner_control(*this, leftPane, rightPane, getSettings().game.bloomMultiplier,
            GraphicsTunerProps{
                .option = GraphicsOption::BloomMultiplier,
                .title = "泛光亮度",
                .helpText = kBloomBrightnessHelpText,
                .valueMin = 0,
                .valueMax = 100,
                .defaultValue = 100,
                .step = 10,
            });
        graphics_tuner_control(*this, leftPane, rightPane, getSettings().game.depthOfFieldMode,
            GraphicsTunerProps{
                .option = GraphicsOption::DepthOfFieldMode,
                .title = "景深",
                .helpText = kDepthOfFieldHelpText,
                .valueMin = static_cast<int>(DepthOfFieldMode::Off),
                .valueMax = static_cast<int>(DepthOfFieldMode::Dusk),
                .defaultValue = static_cast<int>(DepthOfFieldMode::Classic),
            });

        leftPane.add_section("渲染");
        graphics_tuner_control(*this, leftPane, rightPane,
            getSettings().game.enableTextureReplacements,
            GraphicsTunerProps{
                .option = GraphicsOption::TextureReplacements,
                .title = "启用贴图替换",
                .helpText = kTextureReplacementHelpText,
                .valueMin = static_cast<int>(false),
                .valueMax = static_cast<int>(true),
                .defaultValue = static_cast<int>(false),
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "解锁帧率",
                .getValue =
                    [] {
                        return kInterpolationModes[static_cast<u8>(getSettings().game.enableFrameInterpolation.getValue())];
                    },
                .isModified =
                    [] {
                        return getSettings().game.enableFrameInterpolation.getValue() !=
                               getSettings().game.enableFrameInterpolation.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < kInterpolationModes.size(); i++) {
                    pane.add_button({
                            .text = kInterpolationModes[i],
                            .isSelected =
                                [i] {
                                    return getSettings().game.enableFrameInterpolation.getValue() == static_cast<FrameInterpMode>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.enableFrameInterpolation.setValue(static_cast<FrameInterpMode>(i));
                            presentation::update_frame_rate_preference();
                            config::save();
                        });
                }
                pane.add_rml(kUnlockFramerateHelpText);
            });
        config_int_select(leftPane, rightPane, getSettings().video.maxFrameRate,
            "帧率上限", "将帧率限制为指定数值。", 30, 540, 1,
            [] { return getSettings().game.enableFrameInterpolation.getValue() != FrameInterpMode::Capped; },
            [](int) { presentation::update_frame_rate_preference(); });
        config_bool_select(leftPane, rightPane, getSettings().game.enableMapBackground,
            {
                .key = "启用小地图阴影",
                .helpText = "在小地图周围渲染较厚的阴影，可能影响性能。"
            });
        config_bool_select(leftPane, rightPane, getSettings().game.disableCutscenePillarboxing,
            {
                .key = "禁用过场黑边",
                .helpText = "禁用部分过场动画（尤其是超宽屏）左右两侧的黑边。超出原始画面范围的内容可能出现显示异常。"
            });
    });

    add_tab("输入", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        auto addOption = [&](const Rml::String& key, ConfigVar<bool>& value,
                             const Rml::String& helpText, std::function<bool()> isDisabled = {}) {
            config_bool_select(leftPane, rightPane, value,
                {
                    .key = key,
                    .helpText = helpText,
                    .isDisabled = std::move(isDisabled),
                });
        };

        leftPane.add_section("输入设置");
        leftPane.register_control(leftPane.add_button("配置按键").on_pressed([this] {
            push(std::make_unique<ControllerConfigWindow>());
        }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text("打开按键绑定配置。");
            });
        config_bool_select(leftPane, rightPane, getSettings().game.allowBackgroundInput,
            {
                .key = "允许后台输入",
                .helpText = "游戏窗口未聚焦时也接受输入。",
                .onChange = [](bool value) { aurora_set_background_input(value); },
            });

#if TOUCH_CONTROLS_AVAILABLE
        leftPane.add_section("触屏");
        addOption("触屏控制", getSettings().game.enableTouchControls,
            "为触屏设备启用控制界面。<br/><br/>在屏幕左侧按住拖动移动角色，在屏幕右侧控制镜头。");
        auto& customizeTouchLayout = leftPane.add_button(ControlledButton::Props{
            .text = "自定义布局",
            .isDisabled = [] { return !getSettings().game.enableTouchControls; },
        });
        leftPane.register_control(customizeTouchLayout.on_pressed(
                                      [this] { push(std::make_unique<TouchControlsEditor>()); }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text("打开触屏控制布局编辑器。");
            });
        leftPane.register_control(leftPane.add_select_button({
                                      .key = "触屏锁定",
                                      .getValue =
                                          [] {
                                              return touch_targeting_label(
                                                  getSettings().game.touchTargeting.getValue());
                                          },
                                      .isDisabled =
                                          [] { return !getSettings().game.enableTouchControls; },
                                      .isModified =
                                          [] {
                                              const auto& targeting =
                                                  getSettings().game.touchTargeting;
                                              return targeting.getValue() !=
                                                     targeting.getDefaultValue();
                                          },
                                  }),
            rightPane, [](Pane& pane) {
                pane.clear();
                for (int i = 0; i < static_cast<int>(kTouchTargetingLabels.size()); ++i) {
                    pane.add_button({
                            .text = kTouchTargetingLabels[i],
                            .isSelected =
                                [i] {
                                    return getSettings().game.touchTargeting.getValue() ==
                                           static_cast<TouchTargeting>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.touchTargeting.setValue(
                                static_cast<TouchTargeting>(i));
                            config::save();
                        });
                }
                pane.add_rml(fmt::format("<br/>混合：{}<br/>按住：{}<br/>切换：{}",
                    kTouchTargetingDescriptions[0], kTouchTargetingDescriptions[1],
                    kTouchTargetingDescriptions[2]));
            });
        config_percent_select(leftPane, rightPane, getSettings().game.touchCameraXSensitivity,
            "触屏镜头 X 灵敏度",
            "调节触屏镜头水平灵敏度。<br/><br/>仅对触屏输入生效。",
            25, 400, 5, [] { return !getSettings().game.enableTouchControls; });
        config_percent_select(leftPane, rightPane, getSettings().game.touchCameraYSensitivity,
            "触屏镜头 Y 灵敏度",
            "调节触屏镜头垂直灵敏度。<br/><br/>仅对触屏输入生效。", 25,
            400, 5, [] { return !getSettings().game.enableTouchControls; });
#endif

        leftPane.add_section("镜头");
        addOption("自由镜头", getSettings().game.freeCamera,
            "启用自由镜头，可通过 C 摇杆完全控制镜头。");
        config_percent_select(leftPane, rightPane, getSettings().game.freeCameraXSensitivity,
            "自由镜头 X 灵敏度",
            "调节自由镜头水平灵敏度。<br/><br/>仅对控制摇杆生效。",
            50, 200, 5, [] { return !getSettings().game.freeCamera; });
        config_percent_select(leftPane, rightPane, getSettings().game.freeCameraYSensitivity,
            "自由镜头 Y 灵敏度",
            "调节自由镜头垂直灵敏度。<br/><br/>仅对控制摇杆生效。",
            50, 200, 5, [] { return !getSettings().game.freeCamera; });
        addOption("反转镜头 X 轴", getSettings().game.invertCameraXAxis,
            "反转镜头水平移动方向。<br/><br/>仅对控制摇杆生效。");
        addOption("反转镜头 Y 轴", getSettings().game.invertCameraYAxis,
            "反转镜头垂直移动方向。<br/><br/>仅对控制摇杆生效。",
            [] { return !getSettings().game.freeCamera; });
        addOption("反转第一人称 X 轴", getSettings().game.invertFirstPersonXAxis,
            "使用道具瞄准或第一人称镜头时反转水平移动。<br/><br/>仅对控制摇杆生效。");
        addOption("反转第一人称 Y 轴", getSettings().game.invertFirstPersonYAxis,
            "使用道具瞄准或第一人称镜头时反转垂直移动。<br/><br/>仅对控制摇杆生效。");

        leftPane.add_section("陀螺仪");
        addOption("陀螺仪瞄准", getSettings().game.enableGyroAim,
            "在观察模式、瞄准鹰或使用支持的道具瞄准时启用陀螺仪控制。<br/><br/>支持的道具包括弹弓、疾风回旋镖、勇者之弓、爪钩、链球和支配之杖。");
        addOption("陀螺仪滚球", getSettings().game.enableGyroRollgoal,
            "在希娜的小屋中为滚球小游戏启用陀螺仪控制。");
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSensitivityY,
            "陀螺仪俯仰灵敏度", "控制陀螺仪垂直瞄准灵敏度。", 25, 400, 5,
            [] { return !gyro_enabled(); });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSensitivityX,
            "陀螺仪偏航灵敏度", "控制陀螺仪水平瞄准灵敏度。", 25, 400, 5,
            [] { return !gyro_enabled(); });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSensitivityRollgoal,
            "滚球灵敏度", "控制陀螺仪输入倾斜滚球台的力度。",
            25, 400, 5,
            [] { return !getSettings().game.enableGyroRollgoal; });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroDeadband, "陀螺仪死区",
            "忽略微小的陀螺仪移动以减少漂移和抖动。", 0, 50, 1,
            [] { return !gyro_enabled(); });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSmoothing,
            "陀螺仪平滑", "数值越高，陀螺仪输入随时间越平滑。", 0, 100, 1,
            [] { return !gyro_enabled(); });
        addOption("反转陀螺仪俯仰", getSettings().game.gyroInvertPitch,
            "反转陀螺仪垂直瞄准方向。", [] { return !gyro_enabled(); });
        addOption("反转陀螺仪偏航", getSettings().game.gyroInvertYaw,
            "反转陀螺仪水平瞄准方向。", [] { return !gyro_enabled(); });

        leftPane.add_section("鼠标");
        addOption("鼠标瞄准", getSettings().game.enableMouseAim,
            "在观察模式、瞄准鹰或使用支持的道具瞄准时启用鼠标输入。<br/><br/>支持的道具包括弹弓、疾风回旋镖、勇者之弓、爪钩、链球和支配之杖。");
        addOption("鼠标镜头", getSettings().game.enableMouseCamera,
            "启用鼠标控制第三人称镜头。");
        config_percent_select(leftPane, rightPane, getSettings().game.mouseAimSensitivity,
            "鼠标瞄准灵敏度", "控制鼠标瞄准灵敏度。", 25, 400, 5,
            [] { return !getSettings().game.enableMouseAim; });
        config_percent_select(leftPane, rightPane, getSettings().game.mouseCameraSensitivity,
            "鼠标镜头灵敏度", "控制鼠标镜头灵敏度。", 25, 400, 5,
            [] { return !getSettings().game.enableMouseCamera; });
        addOption("反转鼠标 Y 轴", getSettings().game.invertMouseY,
            "反转瞄准和镜头控制的鼠标垂直方向。",
            [] { return !getSettings().game.enableMouseAim || !getSettings().game.enableMouseCamera; });

        leftPane.add_section("玩法");
        addOption("菜单中的鼠标/触屏", getSettings().game.enableMenuPointer,
            "为支持的游戏内菜单启用鼠标和触屏输入。");
        addOption("反转飞行/游泳 X 轴", getSettings().game.invertAirSwimX,
            "飞行或游泳时反转水平移动。");
        addOption("反转飞行/游泳 Y 轴", getSettings().game.invertAirSwimY,
            "飞行或游泳时反转垂直移动。");
        addOption("交换直接选择输入", getSettings().game.swapDirectSelect,
            "交换道具轮盘上直接选择的控制方式：直接选择成为默认操作，按住 L 滚动轮盘。");

        leftPane.add_section("工具");
        addOption("加速键", getSettings().game.enableTurboKeybind,
            "按住 Tab 可将游戏速度提升至最多 4 倍。",
            [] { return getSettings().game.speedrunMode.getValue(); });
        addOption("重置键（" + Rml::String{hotkeys::DO_RESET} + "）",
            getSettings().game.enableResetKeybind,
            "按 " + Rml::String{hotkeys::DO_RESET} + " 重置游戏。");
    });

    add_tab("音频", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        // TODO: Individual sliders for Main Music, Sub Music, Sound Effects, and Fanfare.
        leftPane.add_section("音量");
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "主音量",
                .getValue = [] { return getSettings().audio.masterVolume.getValue(); },
                .setValue =
                    [](int value) {
                        getSettings().audio.masterVolume.setValue(value);
                        config::save();
                        audio::SetMasterVolume(audio::MasterVolumeToLinear(value / 100.0f));
                    },
                .isModified =
                    [] {
                        return getSettings().audio.masterVolume.getValue() !=
                               getSettings().audio.masterVolume.getDefaultValue();
                    },
                .max = 100,
                .suffix = "%",
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text("调节游戏中所有声音的音量。");
            });

        leftPane.add_section("音效");
        config_bool_select(leftPane, rightPane, getSettings().audio.enableReverb,
            {
                .key = "启用混响",
                .helpText = "为游戏音频启用混响效果。",
                .onChange = [](bool value) { audio::SetEnableReverb(value); },
            });
        config_bool_select(leftPane, rightPane, getSettings().audio.enableHrtf,
            {
                .key = "启用空间音效",
                .helpText =
                    "通过 HRTF 模拟环绕声，仅建议使用耳机时开启！",
                .onChange = [](bool value) { audio::EnableHrtf = value; },
            });
        config_bool_select(leftPane, rightPane, getSettings().audio.menuSounds,
            {
                .key = "Dusklight 菜单音效",
                .helpText = "在浏览 Dusklight 菜单时播放音效。",
            });

        leftPane.add_section("微调");
        config_bool_select(leftPane, rightPane, getSettings().game.noLowHpSound,
            {
                .key = "无低血量提示音",
                .helpText = "血量较低时禁用提示音。",
            });
        config_bool_select(leftPane, rightPane, getSettings().game.midnasLamentNonStop,
            {
                .key = "米德娜哀歌不间断",
                .helpText = "播放米德娜哀歌时停止战斗音乐。",
            });
    });

    add_tab("玩法", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        auto addOption = [&](const Rml::String& key, ConfigVar<bool>& value,
                             const Rml::String& helpText) {
            config_bool_select(leftPane, rightPane, value,
                {
                    .key = key,
                    .helpText = helpText,
                });
        };
        auto addSpeedrunDisabledOption = [&](const Rml::String& key, ConfigVar<bool>& value,
                                             const Rml::String& helpText) {
            add_speedrun_disabled_option(leftPane, rightPane, value, key, helpText);
        };

        leftPane.add_section("常规");
        addOption("镜像模式", getSettings().game.enableMirrorMode,
            "水平镜像整个世界，与 Wii 版本一致。");
        addOption("极简 HUD", getSettings().game.minimalHUD,
            "禁用游戏主 HUD 的各个元素。<br/>有助于获得更沉浸的体验。");
        config_percent_select(leftPane, rightPane, getSettings().game.hudScale,
            "HUD 缩放",
            "缩放游戏 HUD（心、按键、小地图等）的大小，不影响对话框和菜单。",
            50, 200, 5,
            [] { return getSettings().game.minimalHUD.getValue(); });
        addOption("还原 Wii 1.0 漏洞", getSettings().game.restoreWiiGlitches,
            "还原首个发售版本 Wii 美版 1.0 中已修复的漏洞。");
        addOption("启用林克模型旋转", getSettings().game.enableLinkDollRotation,
            "在收藏菜单中可用 C 摇杆旋转林克模型。");
        addOption("隐藏猫头鹰雕像标记", getSettings().game.removeQuestMapMarkers,
            "从地图和小地图中移除已完成猫头鹰雕像的标记。");

        leftPane.add_section("难度");
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = "伤害倍率",
                .getValue = [] { return getSettings().game.damageMultiplier.getValue(); },
                .setValue =
                    [](int value) {
                        getSettings().game.damageMultiplier.setValue(value);
                        config::save();
                    },
                .isDisabled = [] { return getSettings().game.speedrunMode.getValue(); },
                .isModified =
                    [] {
                        return getSettings().game.damageMultiplier.getValue() !=
                               getSettings().game.damageMultiplier.getDefaultValue();
                    },
                .min = 1,
                .max = 8,
                .suffix = "×",
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text("倍乘受到的伤害。");
            });
        addSpeedrunDisabledOption(
            "一击必杀", getSettings().game.instantDeath, "任何攻击都会立即击杀你。");
        addSpeedrunDisabledOption("无心心掉落", getSettings().game.noHeartDrops,
            "敌人、罐子等将不再掉落心心。");

        leftPane.add_section("便利功能");
        addOption("更大的钱包", getSettings().game.biggerWallets,
            "钱包容量与 HD 版一致（500、1000、2000）。");
        addOption("禁用卢比过场", getSettings().game.disableRupeeCutscenes,
            "首次获得卢比后将不再播放过场动画。");
        addOption("更快攀爬", getSettings().game.fastClimbing,
            "与 HD 版一样更快地攀爬梯子和藤蔓。");
        addOption("更快的光之泪", getSettings().game.fastTears,
            "与 HD 版一样，影之虫掉落的光之泪弹出更快。");
        addSpeedrunDisabledOption("自动存档", getSettings().game.autoSave,
            "进入新区域或打开迷宫门时自动存档。");
        addOption("即时存档", getSettings().game.instantSaves,
            "跳过写入记忆卡的延迟。");
        addOption("按住 B 即时显示文字", getSettings().game.instantText,
            "按住 B 使文字立即滚动。");
        addOption("无攀爬失误动画", getSettings().game.noMissClimbing,
            "林克抓住边缘或攀爬藤蔓时不再播放挣扎动画。");
        addOption("卢比不回退", getSettings().game.noReturnRupees,
            "即使钱包已满也始终拾取卢比。");
        addOption("剑无后仰", getSettings().game.noSwordRecoil,
            "林克的剑击中墙壁时不再后仰。");
        addOption("无需第二条鱼给猫", getSettings().game.no2ndFishForCat,
            "跳过为塞拉的猫捕捉第二条鱼的要求。");
        addOption("按键钓鱼", getSettings().game.buttonFishing,
            "允许使用鱼竿所绑定的按键进行钓鱼。");
        addOption("在地图上显示鬼魂数量", getSettings().game.enhancedMapMenus,
            "在地图上显示区域内已收集/总共的波伊之魂数量。");
        addSpeedrunDisabledOption("太阳之歌（R+X）", getSettings().game.sunsSong,
            "允许狼林克嚎叫并改变时间。");
        addOption("快速变身（R+Y）", getSettings().game.enableQuickTransform,
            "同时按下 R 和 Y 立即变身。");

        leftPane.add_section("速通");
        config_bool_select(leftPane, rightPane, getSettings().game.speedrunMode,
            {
                .key = "速通模式",
                .helpText =
                    "启用速通选项，同时限制部分玩法修改项。",
                .onChange =
                    [](bool enabled) {
                        if (enabled) {
                            resetForSpeedrunMode();
                        } else {
                            restoreFromSpeedrunMode();
                            if (getSettings().game.liveSplitEnabled) {
                                speedrun::disconnectLiveSplit();
                            }
                        }
                        MenuBar::rebuild();
                    },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.liveSplitEnabled,
            {
                .key = "LiveSplit 连接",
                .helpText = "连接 localhost:16834 上的 LiveSplit 服务器。需要在 LiveSplit 上右键，开启 Control → Start TCP Server。要在 LiveSplit 中查看 IGT，需将比较方式改为 Game Time。",
                .onChange =
                    [](bool enabled) {
                        if (enabled) {
                            speedrun::connectLiveSplit();
                        } else {
                            speedrun::disconnectLiveSplit();
                        }
                    },
                .isDisabled = [] { return IsMobile || !getSettings().game.speedrunMode; },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showSpeedrunRTATimer,
            {
                .key = "显示 RTA",
                .helpText = "显示 RTA 计时器，IGT 始终可见。",
                .isDisabled = [] { return !getSettings().game.speedrunMode; },
            });
    });

    add_tab("作弊", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        auto addCheat = [&](const Rml::String& key, ConfigVar<bool>& value,
                            const Rml::String& helpText) {
            add_speedrun_disabled_option(leftPane, rightPane, value, key, helpText);
        };

        leftPane.add_section("资源");
        addCheat("无限心心", getSettings().game.infiniteHearts, "始终保持满血。");
        addCheat("无限箭矢", getSettings().game.infiniteArrows, "始终保持箭矢满额。");
        addCheat("无限种子", getSettings().game.infiniteSeeds, "始终保持弹弓弹丸（种子）满额。");
        addCheat("无限炸弹", getSettings().game.infiniteBombs, "始终保持所有炸弹袋满额。");
        addCheat("无限灯油", getSettings().game.infiniteOil, "始终保持提灯灯油满额。");
        addCheat("无限氧气", getSettings().game.infiniteOxygen,
            "始终保持水下氧气值满额。");
        addCheat("无限卢比", getSettings().game.infiniteRupees, "始终保持卢比满额。");
        addCheat("物品无消失计时", getSettings().game.enableIndefiniteItemDrops,
            "卢比、心心等掉落物品将不会消失。");

        leftPane.add_section("能力");
        addCheat("月球跳跃（R+A）", getSettings().game.moonJump, "按住 R 和 A 升入空中。");
        addCheat("超级爪钩", getSettings().game.superClawshot,
            "让爪钩行为超越常规游戏规则。");
        addCheat("始终可大回旋斩", getSettings().game.alwaysGreatspin,
            "无需满血即可使用大回旋斩。");
        addCheat("快速铁靴", getSettings().game.enableFastIronBoots,
            "加重状态（穿铁靴、持链球、无卢比穿魔法铠甲等）下移动更快。");
        addCheat("任意地点变身", getSettings().game.canTransformAnywhere,
            "即使有 NPC 注视也可以变身。");
        addCheat("快速翻滚", getSettings().game.fastRoll,
            "林克的翻滚动画和移动速度加倍。");
        addCheat("快速陀螺", getSettings().game.fastSpinner,
            "按住 R 时陀螺移动更快。");
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "魔法铠甲行为",
                .getValue =
                    [] {
                        return kMagicArmorModes[static_cast<u8>(getSettings().game.armorRupeeDrain.getValue())];
                    },
                .isDisabled = [] { return getSettings().game.speedrunMode.getValue(); },
                .isModified =
                    [] {
                        return getSettings().game.armorRupeeDrain.getValue() !=
                               getSettings().game.armorRupeeDrain.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < kMagicArmorModes.size(); i++) {
                    pane.add_button({
                            .text = kMagicArmorModes[i],
                            .isSelected =
                                [i] {
                                    return getSettings().game.armorRupeeDrain.getValue() == static_cast<MagicArmorMode>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.armorRupeeDrain.setValue(static_cast<MagicArmorMode>(i));
                            config::save();
                        });
                }
                pane.add_rml(
                    "<br/>控制魔法铠甲的行为。");
            });
        addCheat("敌人无敌", getSettings().game.invincibleEnemies,
            "敌人不会受到伤害。");
    });

    add_tab("界面", [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section("Dusklight");
#if DUSK_CAN_OPEN_DATA_FOLDER
        leftPane.register_control(
            leftPane.add_button("打开数据文件夹").on_pressed([] {
                mDoAud_seStartMenu(kSoundClick);
                data::open_data_path();
            }),
            rightPane, [](Pane& pane) {
                pane.add_text(
                    "打开 Dusklight 存放设置、存档、日志、贴图替换等应用数据的文件夹。");
            });
#endif
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "通知",
                .getValue = [] {
                    const bool ach = getSettings().game.enableAchievementToasts.getValue();
                    const bool ctl = getSettings().game.enableControllerToasts.getValue();
                    if (!ach && !ctl) {
                        return Rml::String{"关闭"};
                    }
                    if (ach && ctl) {
                        return Rml::String{"全部"};
                    }
                    return Rml::String{"部分"};
                },
                .isModified = [] {
                    const auto& ach = getSettings().game.enableAchievementToasts;
                    const auto& ctl = getSettings().game.enableControllerToasts;
                    return ach.getValue() != ach.getDefaultValue() || ctl.getValue() != ctl.getDefaultValue();
                },
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_button("全选").on_pressed([] {
                    mDoAud_seStartMenu(kSoundItemChange);
                    getSettings().game.enableAchievementToasts.setValue(true);
                    getSettings().game.enableControllerToasts.setValue(true);
                    config::save();
                });
                pane.add_button("全不选").on_pressed([] {
                    mDoAud_seStartMenu(kSoundItemChange);
                    getSettings().game.enableAchievementToasts.setValue(false);
                    getSettings().game.enableControllerToasts.setValue(false);
                    config::save();
                });

                pane.add_section("类型");
                pane.add_button(
                    {
                        .text = "成就",
                        .isSelected =
                        [] {
                            return getSettings().game.enableAchievementToasts.getValue();
                        },
                    })
                    .on_pressed([] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        auto& v = getSettings().game.enableAchievementToasts;
                        v.setValue(!v.getValue());
                        config::save();
                    });
                pane.add_button(
                    {
                        .text = "设备断开",
                        .isSelected =
                            [] { return getSettings().game.enableControllerToasts.getValue(); },
                    })
                    .on_pressed([] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        auto& v = getSettings().game.enableControllerToasts;
                        v.setValue(!v.getValue());
                        config::save();
                    });
                pane.add_rml("<br/>选择可以显示的通知类型。");
            });
#if BOREALIS_HAS_SENTRY
        auto& crashReporting = leftPane.add_child<BoolButton>(BoolButton::Props{
            .key = "崩溃报告",
            .getValue =
                [] { return borealis::sentry::get_consent() == borealis::sentry::Consent::Given; },
            .setValue = [](bool enabled) { borealis::sentry::set_consent(enabled); },
            .isDisabled =
                [] {
                    return borealis::sentry::get_consent() ==
                           borealis::sentry::Consent::Unavailable;
                },
            .isModified = [] { return false; },
        });
        leftPane.register_control(crashReporting, rightPane, [](Pane& pane) {
            pane.clear();
            pane.add_rml("Dusklight 可以向开发者自动发送崩溃报告。崩溃报告包含以下内容：<br/>• "
                         "操作系统版本<br/>• CPU 架构<br/>• GPU 型号与驱动版本<br/>• 文件路径（可能"
                         "包含账户用户名）<br/>• 堆栈信息");
        });
#endif
        config_bool_select(leftPane, rightPane, getSettings().backend.skipPreLaunchUI,
            {
                .key = "跳过 Dusklight 主菜单",
                .helpText = "启动 Dusklight 时，如有可用游戏镜像则跳过主菜单直接进入游戏。",
            });
        config_bool_select(leftPane, rightPane, getSettings().backend.checkForUpdates,
            {
                .key = "检查更新",
                .helpText = "启动时检查 GitHub 是否有 Dusklight 新版本。<br/><br/>"
                            "不会传输或收集任何个人信息。",
            });
#if BOREALIS_HAS_DISCORD
        config_bool_select(leftPane, rightPane, getSettings().game.enableDiscordPresence,
            {
                .key = "启用 Discord 状态展示",
                .helpText = "允许 Dusklight 与 Discord 状态展示集成，让 Discord 显示你的游戏内状态。",
                .onChange = [](bool enabled) {
                    if (enabled) {
                        dusk::discord::initialize();
                    } else {
                        dusk::discord::shutdown();
                    }
                },
            });
#endif
        config_bool_select(leftPane, rightPane, getSettings().backend.enableAdvancedSettings,
            {
                .key = "启用高级设置",
                .icon = "warning",
                .helpText = "使用 Shift+F1 显示高级设置和调试工具。<br/><br/><icon class=\"warning\"/> "
                            "警告：调试工具很容易损坏你的游戏。请勿在常规存档上使用！",
                .onChange = [](bool) { MenuBar::rebuild(); },
                .isDisabled = [] { return getSettings().game.speedrunMode.getValue(); },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewer,
            {
                .key = "显示输入查看器",
                .helpText = "游戏过程中显示手柄输入叠加层。",
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewerGyro,
            {
                .key = "显示陀螺仪输入查看器",
                .helpText = "在输入查看器中显示陀螺仪传感器数值。",
                .isDisabled = [] { return !getSettings().game.showInputViewer; },
            });
        leftPane.add_section("游戏");
        leftPane.register_control(
            leftPane.add_select_button({
                .key = "菜单缩放模式",
                .getValue =
                    [] {
                        return kMenuScalingModeLabels[static_cast<u8>(
                            getSettings().game.menuScalingMode.getValue())];
                    },
                .isModified =
                    [] {
                        const auto& mode = getSettings().game.menuScalingMode;
                        return mode.getValue() != mode.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < static_cast<int>(kMenuScalingModeLabels.size()); ++i) {
                    pane
                        .add_button({
                            .text = kMenuScalingModeLabels[i],
                            .isSelected =
                                [i] {
                                    return getSettings().game.menuScalingMode.getValue() ==
                                           static_cast<MenuScaling>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.menuScalingMode.setValue(
                                static_cast<MenuScaling>(i));
                            config::save();
                        });
                }
                pane.add_rml("<br/>更改收藏和文件选择菜单随宽高比缩放的方式。");
            });
        config_bool_select(leftPane, rightPane, getSettings().game.hideTvSettingsScreen,
            {
                .key = "跳过电视设置画面",
                .helpText = "跳过读取存档时显示的电视校准画面。",
            });
        add_speedrun_disabled_option(leftPane, rightPane, getSettings().game.recordingMode,
            "录制模式",
            "禁用游戏 HUD 和所有背景音乐。<br/><br/>方便录制视频。");
    });
}

void SettingsWindow::update() {
    if (mPrelaunch && top_document() == this) {
        try_push_verification_modal(*this);
    }

    Window::update();
}

void SettingsWindow::hide(bool close) {
    config::save();
    Window::hide(close);
}

}  // namespace dusk::ui

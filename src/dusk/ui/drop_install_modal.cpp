#include "drop_install_modal.hpp"

#include "dusk/mods/loader/loader.hpp"
#include "dusk/mods/loader/packages.hpp"
#include "dusk/mods/queue.hpp"
#include "format.hpp"
#include "mods_window.hpp"
#include "package_row.hpp"

#include <borealis/io.hpp>
#include <borealis/update.hpp>
#include <fmt/format.h>

#include <algorithm>

namespace dusk::ui {
namespace {

size_t valid_count(const std::vector<DropPackage>& packages) {
    return std::ranges::count(packages, true, &DropPackage::valid);
}

std::vector<DropPackage> prepare_packages(std::vector<DropPackage> packages) {
    std::vector<std::string> batchIds;
    for (auto& package : packages) {
        if (!package.error.empty()) {
            package.status = package.error;
        } else if (std::ranges::find(batchIds, package.metadata.id) != batchIds.end()) {
            package.status = "本次拖放中存在重复模组包";
        } else if (!borealis::update::parse_version(package.metadata.version)) {
            package.status = "无效的模组包版本";
        } else if (package.hasNative && !mods::EnableCodeMods) {
            package.status = "本平台无法安装原生模组";
        } else if (const auto queued = mods::queue::find_by_mod_id(package.metadata.id);
            queued && !mods::queue::is_terminal(queued->state))
        {
            package.status = "已在安装队列中";
        } else if (const auto* installed =
                       mods::ModLoader::instance().find_mod(package.metadata.id))
        {
            if (!mods::ModLoader::instance().can_update(*installed)) {
                package.status = "开发目录不能被替换";
            } else if (mods::compare_package_versions(
                           package.metadata.version, installed->metadata.version) < 0) {
                package.status = "已安装更新版本";
            } else if (mods::compare_package_versions(
                           package.metadata.version, installed->metadata.version) == 0) {
                package.status = fmt::format("重新安装 {}", package.metadata.version);
                package.valid = true;
            } else {
                package.status = fmt::format("从 {} 更新", installed->metadata.version);
                package.valid = true;
            }
        } else {
            package.status = "新";
            package.valid = true;
        }
        batchIds.push_back(package.metadata.id);
    }
    return packages;
}

}  // namespace

std::vector<DropPackage> inspect_drop_packages(
    const std::vector<std::filesystem::path>& paths, borealis::TaskContext& context) {
    std::vector<DropPackage> packages;
    packages.reserve(paths.size());
    for (const auto& path : paths) {
        if (context.cancel_requested()) {
            break;
        }
        DropPackage package{.path = path};
        std::error_code error;
        package.size = std::filesystem::file_size(path, error);
        if (error) {
            package.error = fmt::format("无法读取模组包：{}", error.message());
        } else if (!mods::inspect_mod_bundle(
                       path, package.metadata, package.error, &package.hasNative))
        {
            package.error = fmt::format("无效模组包：{}", package.error);
        }
        packages.push_back(std::move(package));
        context.report_progress(packages.size(), paths.size());
    }
    return packages;
}

DropInstallModal::DropInstallModal(std::vector<DropPackage> packages)
    : DropInstallModal{prepare_packages(std::move(packages)), PreparedTag{}} {}

DropInstallModal::DropInstallModal(std::vector<DropPackage> packages, PreparedTag)
    : Modal{Props{
          .title = "安装模组？",
          .bodyText = "仅安装来自可信作者的模组。",
          .actions =
              {
                  ModalAction{
                      .label = "取消",
                      .onPressed = [](Modal& modal) { modal.pop(); },
                      .isDisabled = {},
                  },
                  ModalAction{
                      .label = fmt::format("安装 {}", valid_count(packages)),
                      .onPressed = [this](Modal&) { install(); },
                      .isDisabled = [this] { return valid_count(mPackages) == 0; },
                  },
              },
          .variant = "drop-install",
      }},
      mPackages{std::move(packages)} {
    auto& pane = content_pane();
    for (auto& package : mPackages) {
        auto& row = pane.add_child<PackageRow>();
        const auto name = package.metadata.name.empty() ?
                              borealis::io::fs_path_to_string(package.path.filename()) :
                              package.metadata.name;
        const auto version =
            package.metadata.name.empty() ? std::string{} : package.metadata.version;
        const auto detail =
            package.metadata.author.empty() ?
                format_bytes(package.size) :
                fmt::format("{} · {}", package.metadata.author, format_bytes(package.size));
        row.set_package(name, version, package.status, detail, package.valid ? "queued" : "failed");
        row.set_disabled(!package.valid);
    }
}

void DropInstallModal::install() {
    std::string firstKey;
    for (const auto& package : mPackages) {
        if (!package.valid) {
            continue;
        }
        std::string key;
        if (mods::queue::enqueue(
                {
                    .id = package.metadata.id,
                    .name = package.metadata.name,
                    .version = package.metadata.version,
                    .source = mods::queue::LocalFile{package.path},
                },
                &key) &&
            firstKey.empty())
        {
            firstKey = std::move(key);
        }
    }
    pop();
    if (!firstKey.empty()) {
        show_online_mods(std::move(firstKey));
    }
}

}  // namespace dusk::ui

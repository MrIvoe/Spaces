#include "SpaceRepository.h"

#include "AppPaths.h"
#include "AppVersion.h"
#include "JsonPersistence.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <system_error>

namespace {

int ReadIniInt(const std::wstring& section, const std::wstring& key, int defaultValue, const std::filesystem::path& path) {
    return static_cast<int>(GetPrivateProfileIntW(section.c_str(), key.c_str(), defaultValue, path.c_str()));
}

std::wstring ReadIniString(const std::wstring& section, const std::wstring& key, const std::wstring& defaultValue, const std::filesystem::path& path) {
    wchar_t buffer[512]{};
    GetPrivateProfileStringW(section.c_str(), key.c_str(), defaultValue.c_str(), buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
    return buffer;
}

nlohmann::json RectToJson(const RECT& rect) {
    return {
        {"x", rect.left},
        {"y", rect.top},
        {"width", rect.right - rect.left},
        {"height", rect.bottom - rect.top},
    };
}

RECT RectFromJson(const nlohmann::json& jsonRect) {
    const int x = jsonRect.value("x", 40);
    const int y = jsonRect.value("y", 40);
    const int width = std::max(180, jsonRect.value("width", 360));
    const int height = std::max(120, jsonRect.value("height", 300));
    return RECT{x, y, x + width, y + height};
}

const char* DropPolicyToString(DropPolicy policy) {
    switch (policy) {
    case DropPolicy::Move:
        return "move";
    case DropPolicy::Copy:
        return "copy";
    case DropPolicy::Prompt:
        return "prompt";
    }

    return "move";
}

DropPolicy DropPolicyFromString(const std::string& value) {
    if (value == "copy") {
        return DropPolicy::Copy;
    }
    if (value == "prompt") {
        return DropPolicy::Prompt;
    }
    return DropPolicy::Move;
}

const char* SpaceTypeToString(SpaceType type) {
    switch (type) {
    case SpaceType::Standard:
        return "standard";
    case SpaceType::Portal:
        return "portal";
    }

    return "standard";
}

SpaceType SpaceTypeFromString(const std::string& value) {
    if (value == "portal") {
        return SpaceType::Portal;
    }
    return SpaceType::Standard;
}

const char* DesktopItemSourceToString(DesktopItemSource source) {
    switch (source) {
    case DesktopItemSource::Desktop:
        return "desktop";
    case DesktopItemSource::LegacySpaceFolder:
        return "legacy-folder";
    case DesktopItemSource::PortalFolder:
        return "portal-folder";
    }

    return "desktop";
}

DesktopItemSource DesktopItemSourceFromString(const std::string& value) {
    if (value == "legacy-folder") {
        return DesktopItemSource::LegacySpaceFolder;
    }
    if (value == "portal-folder") {
        return DesktopItemSource::PortalFolder;
    }
    return DesktopItemSource::Desktop;
}

} // namespace

SpaceRepository::SpaceRepository()
    : SpaceRepository(AppPaths::GetAppDataRoot()) {
}

SpaceRepository::SpaceRepository(std::filesystem::path basePath)
    : m_basePath(std::move(basePath)) {
}

bool SpaceRepository::Load(SpaceRepositoryState& state) const {
    SpaceRepositoryState loadedState;
    bool loaded = false;

    if (std::filesystem::exists(GetConfigPath())) {
        loaded = LoadFromJson(loadedState);
    } else if (std::filesystem::exists(GetLegacyIniPath())) {
        loaded = LoadFromLegacyIni(loadedState);
    }

    if (!loaded) {
        state = SpaceRepositoryState{};
        state.monitorSignature = BuildMonitorSignature();
        return false;
    }

    const bool migrated = MigrateLegacyFolderBackedSpaces(loadedState);

    const std::wstring savedSignature = loadedState.monitorSignature;
    const std::wstring expectedSignature = BuildMonitorSignature();
    if (!savedSignature.empty() && savedSignature != expectedSignature) {
        loadedState.spaces.clear();
    }

    loadedState.monitorSignature = expectedSignature;
    state = std::move(loadedState);

    if (migrated) {
        Save(state);
    }

    return true;
}

bool SpaceRepository::Save(const SpaceRepositoryState& state) const {
    nlohmann::json root;
    root["schemaVersion"] = 2;
    root["appVersion"] = AppVersion::kCurrentUtf8;
    root["monitorSignature"] = JsonPersistence::ToUtf8(state.monitorSignature.empty() ? BuildMonitorSignature() : state.monitorSignature);
    root["settings"] = {
        {"dropPolicy", DropPolicyToString(state.dropPolicy)},
        {"showInfoNotifications", state.showInfoNotifications},
    };
    root["spaces"] = nlohmann::json::array();

    for (const SpaceData& space : state.spaces) {
        nlohmann::json members = nlohmann::json::array();
        for (const DesktopItemRef& member : space.members) {
            members.push_back({
                {"id", JsonPersistence::ToUtf8(member.itemId)},
                {"name", JsonPersistence::ToUtf8(member.displayName)},
                {"sourcePath", JsonPersistence::ToUtf8(member.sourcePath)},
                {"source", DesktopItemSourceToString(member.source)},
                {"isFolder", member.isFolder},
                {"exists", member.exists},
                {"iconIndex", member.iconIndex},
            });
        }

        root["spaces"].push_back({
            {"id", space.id},
            {"title", JsonPersistence::ToUtf8(space.title)},
            {"rect", RectToJson(space.rect)},
            {"collapsed", space.collapsed},
            {"type", SpaceTypeToString(space.type)},
            {"portalFolder", JsonPersistence::ToUtf8(space.portalFolder)},
            {"backingFolder", JsonPersistence::ToUtf8(space.backingFolder)},
            {"members", std::move(members)},
        });
    }

    return JsonPersistence::SaveJsonFileAtomic(GetConfigPath(), root);
}

std::filesystem::path SpaceRepository::GetConfigPath() const {
    return m_basePath / "spaces.json";
}

std::filesystem::path SpaceRepository::GetSpaceDataRoot() const {
    return m_basePath / "SpaceStorage";
}

std::filesystem::path SpaceRepository::GetLegacyIniPath() const {
    return AppPaths::GetLegacyIniPath();
}

std::wstring SpaceRepository::BuildMonitorSignature() {
    const int monitors = GetSystemMetrics(SM_CMONITORS);
    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    wchar_t signature[128]{};
    swprintf_s(signature, L"m=%d;x=%d;y=%d;w=%d;h=%d", monitors, x, y, w, h);
    return signature;
}

bool SpaceRepository::LoadFromJson(SpaceRepositoryState& state) const {
    nlohmann::json root;
    if (!JsonPersistence::TryLoadJsonFile(GetConfigPath(), root)) {
        return false;
    }

    state = SpaceRepositoryState{};
    state.monitorSignature = JsonPersistence::FromUtf8(root.value("monitorSignature", std::string{}));

    if (root.contains("settings") && root["settings"].is_object()) {
        const nlohmann::json& settings = root["settings"];
        state.dropPolicy = DropPolicyFromString(settings.value("dropPolicy", std::string{"move"}));
        state.showInfoNotifications = settings.value("showInfoNotifications", true);
    }

    if (!root.contains("spaces") || !root["spaces"].is_array()) {
        return true;
    }

    for (const nlohmann::json& spaceJson : root["spaces"]) {
        SpaceData space;
        space.id = spaceJson.value("id", 0);
        space.title = JsonPersistence::FromUtf8(spaceJson.value("title", std::string{"Space"}));
        if (space.title.empty()) {
            space.title = L"Space";
        }
        if (spaceJson.contains("rect") && spaceJson["rect"].is_object()) {
            space.rect = RectFromJson(spaceJson["rect"]);
        }
        space.collapsed = spaceJson.value("collapsed", false);
        space.type = SpaceTypeFromString(spaceJson.value("type", std::string{"standard"}));
        space.portalFolder = JsonPersistence::FromUtf8(spaceJson.value("portalFolder", std::string{}));
        space.backingFolder = JsonPersistence::FromUtf8(spaceJson.value("backingFolder", std::string{}));

        if (spaceJson.contains("members") && spaceJson["members"].is_array()) {
            for (const nlohmann::json& memberJson : spaceJson["members"]) {
                DesktopItemRef member;
                member.itemId = JsonPersistence::FromUtf8(memberJson.value("id", std::string{}));
                member.displayName = JsonPersistence::FromUtf8(memberJson.value("name", std::string{}));
                member.sourcePath = JsonPersistence::FromUtf8(memberJson.value("sourcePath", std::string{}));
                member.source = DesktopItemSourceFromString(memberJson.value("source", std::string{"desktop"}));
                member.isFolder = memberJson.value("isFolder", false);
                member.exists = memberJson.value("exists", true);
                member.iconIndex = memberJson.value("iconIndex", -1);
                if (member.displayName.empty() && !member.sourcePath.empty()) {
                    member.displayName = std::filesystem::path(member.sourcePath).filename().wstring();
                }
                if (member.itemId.empty()) {
                    member.itemId = member.sourcePath;
                }
                space.members.push_back(std::move(member));
            }
        }

        state.spaces.push_back(std::move(space));
    }

    return true;
}

bool SpaceRepository::LoadFromLegacyIni(SpaceRepositoryState& state) const {
    const std::filesystem::path iniPath = GetLegacyIniPath();
    state = SpaceRepositoryState{};
    state.dropPolicy = ParseDropPolicy(ReadIniInt(L"General", L"DropPolicy", static_cast<int>(DropPolicy::Move), iniPath));
    state.showInfoNotifications = ReadIniInt(L"General", L"ShowInfoNotifications", 1, iniPath) != 0;
    state.monitorSignature = ReadIniString(L"General", L"MonitorSignature", L"", iniPath);

    const std::wstring expectedSignature = BuildMonitorSignature();
    if (!state.monitorSignature.empty() && state.monitorSignature != expectedSignature) {
        state.spaces.clear();
        return true;
    }

    const int count = ReadIniInt(L"General", L"SpaceCount", 0, iniPath);
    for (int index = 0; index < count; ++index) {
        wchar_t section[32]{};
        swprintf_s(section, L"Space%d", index + 1);

        SpaceData space;
        space.id = ReadIniInt(section, L"Id", index + 1, iniPath);
        space.title = ReadIniString(section, L"Title", L"Space", iniPath);
        space.rect.left = ReadIniInt(section, L"X", 40, iniPath);
        space.rect.top = ReadIniInt(section, L"Y", 40, iniPath);
        space.rect.right = space.rect.left + ReadIniInt(section, L"W", 360, iniPath);
        space.rect.bottom = space.rect.top + ReadIniInt(section, L"H", 300, iniPath);
        space.collapsed = ReadIniInt(section, L"Collapsed", 0, iniPath) != 0;
        space.backingFolder = ReadIniString(section, L"BackingFolder", L"", iniPath);
        state.spaces.push_back(std::move(space));
    }

    return true;
}

bool SpaceRepository::MigrateLegacyFolderBackedSpaces(SpaceRepositoryState& state) const {
    bool changed = false;
    for (SpaceData& space : state.spaces) {
        if (!space.members.empty()) {
            continue;
        }

        if (space.backingFolder.empty()) {
            continue;
        }

        space.members = ScanFolderMembers(space.backingFolder);
        if (!space.members.empty()) {
            changed = true;
        }
    }

    return changed;
}

std::vector<DesktopItemRef> SpaceRepository::ScanFolderMembers(const std::wstring& folderPath) {
    std::vector<DesktopItemRef> members;
    std::error_code error;
    if (folderPath.empty() || !std::filesystem::exists(folderPath, error) || error) {
        return members;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folderPath, error)) {
        if (error) {
            break;
        }

        DesktopItemRef member;
        member.sourcePath = entry.path().wstring();
        member.itemId = member.sourcePath;
        member.displayName = entry.path().filename().wstring();
        member.source = DesktopItemSource::LegacySpaceFolder;
        member.isFolder = entry.is_directory(error);
        member.exists = true;
        members.push_back(std::move(member));
    }

    std::sort(members.begin(), members.end(), [](const DesktopItemRef& left, const DesktopItemRef& right) {
        return _wcsicmp(left.displayName.c_str(), right.displayName.c_str()) < 0;
    });

    return members;
}

DropPolicy SpaceRepository::ParseDropPolicy(int value) {
    if (value == static_cast<int>(DropPolicy::Copy)) {
        return DropPolicy::Copy;
    }
    if (value == static_cast<int>(DropPolicy::Prompt)) {
        return DropPolicy::Prompt;
    }
    return DropPolicy::Move;
}

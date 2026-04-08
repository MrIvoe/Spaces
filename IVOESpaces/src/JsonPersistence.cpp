#include "JsonPersistence.h"

#include <windows.h>

#include <fstream>
#include <system_error>

std::string JsonPersistence::ToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string buffer(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), buffer.data(), required, nullptr, nullptr);
    return buffer;
}

std::wstring JsonPersistence::FromUtf8(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return {};
    }

    std::wstring buffer(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), buffer.data(), required);
    return buffer;
}

bool JsonPersistence::TryLoadJsonFile(const std::filesystem::path& path, nlohmann::json& outJson) {
    if (!std::filesystem::exists(path)) {
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    try {
        input >> outJson;
        return true;
    } catch (...) {
        return false;
    }
}

bool JsonPersistence::SaveJsonFileAtomic(const std::filesystem::path& path, const nlohmann::json& json) {
    std::error_code createError;
    std::filesystem::create_directories(path.parent_path(), createError);
    if (createError) {
        return false;
    }

    const std::filesystem::path tempPath = path.parent_path() / (path.filename().wstring() + L".tmp");
    {
        std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            return false;
        }

        output << json.dump(2);
        output.flush();
        if (!output) {
            output.close();
            std::error_code removeError;
            std::filesystem::remove(tempPath, removeError);
            return false;
        }
    }

    if (!MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code removeError;
        std::filesystem::remove(tempPath, removeError);
        return false;
    }

    return true;
}
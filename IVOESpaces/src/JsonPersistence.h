#pragma once

#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace JsonPersistence {

std::string ToUtf8(const std::wstring& value);
std::wstring FromUtf8(const std::string& value);
bool TryLoadJsonFile(const std::filesystem::path& path, nlohmann::json& outJson);
bool SaveJsonFileAtomic(const std::filesystem::path& path, const nlohmann::json& json);

} // namespace JsonPersistence
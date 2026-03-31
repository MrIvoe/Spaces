#pragma once

#include <filesystem>

namespace AppPaths {

std::filesystem::path GetAppDataRoot();
std::filesystem::path GetConfigPath();
std::filesystem::path GetFenceDataRoot();
std::filesystem::path GetLegacyIniPath();

} // namespace AppPaths
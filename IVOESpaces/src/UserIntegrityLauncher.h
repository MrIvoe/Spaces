#pragma once
#include <string>

class UserIntegrityLauncher {
public:
    static bool IsCurrentProcessElevated();
    static bool LaunchMoveWithUserIntegrity(const std::wstring& sourcePath, const std::wstring& destinationDir);
};

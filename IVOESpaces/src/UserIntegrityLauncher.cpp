#include "UserIntegrityLauncher.h"
#include <windows.h>
#include <shlwapi.h>
#include <vector>

namespace {

std::wstring GetModuleFolder() {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    return exePath;
}

std::wstring BuildHelperPath() {
    return GetModuleFolder() + L"\\UserActionHelper.exe";
}

bool GetExplorerPrimaryToken(HANDLE* outToken) {
    if (!outToken) {
        return false;
    }

    *outToken = nullptr;
    HWND shell = GetShellWindow();
    if (!shell) {
        return false;
    }

    DWORD explorerPid = 0;
    GetWindowThreadProcessId(shell, &explorerPid);
    if (!explorerPid) {
        return false;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, explorerPid);
    if (!process) {
        return false;
    }

    HANDLE explorerToken = nullptr;
    if (!OpenProcessToken(process, TOKEN_DUPLICATE | TOKEN_QUERY, &explorerToken)) {
        CloseHandle(process);
        return false;
    }

    HANDLE primary = nullptr;
    BOOL ok = DuplicateTokenEx(
        explorerToken,
        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID,
        nullptr,
        SecurityImpersonation,
        TokenPrimary,
        &primary);

    CloseHandle(explorerToken);
    CloseHandle(process);

    if (!ok) {
        return false;
    }

    *outToken = primary;
    return true;
}

bool LaunchWithToken(HANDLE token, const std::wstring& helperPath, const std::wstring& args) {
    std::wstring cmd = L"\"" + helperPath + L"\" " + args;
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessWithTokenW(
        token,
        LOGON_WITH_PROFILE,
        helperPath.c_str(),
        mutableCmd.data(),
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!ok) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, 60000);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exitCode == 0;
}

bool LaunchNormally(const std::wstring& helperPath, const std::wstring& args) {
    std::wstring cmd = L"\"" + helperPath + L"\" " + args;
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessW(
        helperPath.c_str(),
        mutableCmd.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!ok) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, 60000);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exitCode == 0;
}

} // namespace

bool UserIntegrityLauncher::IsCurrentProcessElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);

    return ok && elevation.TokenIsElevated != 0;
}

bool UserIntegrityLauncher::LaunchMoveWithUserIntegrity(const std::wstring& sourcePath, const std::wstring& destinationDir) {
    std::wstring helperPath = BuildHelperPath();
    if (!PathFileExistsW(helperPath.c_str())) {
        return false;
    }

    std::wstring args = L"--move \"" + sourcePath + L"\" \"" + destinationDir + L"\"";

    if (!IsCurrentProcessElevated()) {
        return LaunchNormally(helperPath, args);
    }

    HANDLE token = nullptr;
    if (!GetExplorerPrimaryToken(&token)) {
        return false;
    }

    bool ok = LaunchWithToken(token, helperPath, args);
    CloseHandle(token);
    return ok;
}

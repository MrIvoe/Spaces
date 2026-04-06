#include "core/ThemePackageValidator.h"
#include "core/ThemePackageLoader.h"

#include "Win32Helpers.h"

#include <filesystem>
#include <cwctype>
#include <algorithm>

namespace fs = std::filesystem;

ThemePackageValidator::ValidationResult ThemePackageValidator::ValidatePackage(
    const std::wstring& packagePath) const
{
    if (packagePath.empty())
        return ValidationResult::Failure(L"Package path is empty");

    // Check file exists.
    fs::path path(packagePath);
    if (!fs::exists(path))
        return ValidationResult::Failure(L"Package file not found");

    const std::wstring extension = path.extension().wstring();
    if (_wcsicmp(extension.c_str(), L".zip") != 0)
    {
        return ValidationResult::Failure(L"Theme package must be a .zip file");
    }

    // Check size.
    if (!IsValidPackageSize(packagePath))
        return ValidationResult::Failure(L"Package exceeds maximum size (10MB)");

    ThemePackageLoader loader;
    const auto loadResult = loader.LoadPackage(packagePath);
    if (!loadResult.success)
    {
        return ValidationResult::Failure(loadResult.errorMessage.empty()
            ? L"Package extraction/parsing failed"
            : loadResult.errorMessage);
    }

    const auto cleanup = [&]() {
        if (!loadResult.extractedPath.empty())
        {
            ThemePackageLoader::CleanupExtraction(loadResult.extractedPath);
        }
    };

    if (!IsValidThemeIdFormat(loadResult.metadata.themeId))
    {
        cleanup();
        return ValidationResult::Failure(L"theme_id must be kebab-case and 1-100 chars");
    }

    if (ContainsForbiddenContent(loadResult.extractedPath))
    {
        cleanup();
        return ValidationResult::Failure(L"Package contains forbidden content (executables, scripts)");
    }

    // Require minimal token coverage for runtime application.
    const auto& tokens = loadResult.tokenMap.tokens;
    const bool hasWindow = tokens.find(L"win32.base.window-color") != tokens.end() ||
                           tokens.find(L"win32.base.window_color") != tokens.end();
    const bool hasText = tokens.find(L"win32.base.text-color") != tokens.end() ||
                         tokens.find(L"win32.base.text_color") != tokens.end();
    const bool hasAccent = tokens.find(L"win32.base.accent-color") != tokens.end() ||
                           tokens.find(L"win32.base.accent_color") != tokens.end();

    if (!(hasWindow && hasText && hasAccent))
    {
        cleanup();
        return ValidationResult::Failure(L"Token map is missing required base tokens");
    }

    cleanup();
    return ValidationResult::Success(loadResult.metadata.themeId,
                                     loadResult.metadata.displayName,
                                     loadResult.metadata.version);
}

bool ThemePackageValidator::IsValidThemeIdFormat(const std::wstring& themeId)
{
    if (themeId.empty() || themeId.size() > 100)
        return false;

    for (wchar_t c : themeId)
    {
        if (!iswdigit(c) && !iswlower(c) && c != L'-')
            return false;
    }

    // Cannot start or end with hyphen.
    if (themeId.front() == L'-' || themeId.back() == L'-')
        return false;

    // Cannot have consecutive hyphens.
    if (themeId.find(L"--") != std::wstring::npos)
        return false;

    return true;
}

bool ThemePackageValidator::IsValidPackageSize(const std::wstring& packagePath)
{
    try
    {
        fs::path path(packagePath);
        size_t fileSize = fs::file_size(path);
        return fileSize <= (10 * 1024 * 1024); // 10MB max
    }
    catch (...)
    {
        return false;
    }
}

bool ThemePackageValidator::ContainsForbiddenContent(const std::wstring& extractedPath)
{
    // Forbidden file extensions (incomplete list for security).
    const wchar_t* forbiddenExt[] =
    {
        L".exe", L".dll", L".sys", L".scr",
        L".bat", L".cmd", L".com", L".ps1",
        L".vbs", L".js", L".jar",
    };

    std::error_code ec;
    if (!fs::exists(extractedPath, ec))
    {
        return true;
    }

    for (const auto& entry : fs::recursive_directory_iterator(extractedPath, ec))
    {
        if (ec)
        {
            Win32Helpers::LogError(L"Theme package validation failed to enumerate extracted files.");
            return true;
        }

        if (!entry.is_regular_file())
            continue;

        const std::wstring ext = entry.path().extension().wstring();
        for (const auto& forbidden : forbiddenExt)
        {
            if (_wcsicmp(ext.c_str(), forbidden) == 0)
            {
                return true;
            }
        }
    }

    return false;
}

#pragma once

#include <string>
#include <vector>

/// Validates theme packages (.zip archives) for public theme authoring.
/// Ensures packages meet safety, schema, and compatibility requirements.
class ThemePackageValidator
{
public:
    ThemePackageValidator() = default;
    ~ThemePackageValidator() = default;

    /// Result of validation.
    struct ValidationResult
    {
        bool isValid = false;
        std::wstring errorMessage;
        std::wstring themeId;
        std::wstring displayName;
        std::wstring version;

        static ValidationResult Success(const std::wstring& id, const std::wstring& name, const std::wstring& ver)
        {
            return {true, L"", id, name, ver};
        }

        static ValidationResult Failure(const std::wstring& error)
        {
            return {false, error, L"", L"", L""};
        }
    };

    /// Validate a theme package at the given file path (.zip).
    /// Does not extract or modify filesystem.
    ValidationResult ValidatePackage(const std::wstring& packagePath) const;

private:
    /// Validate theme ID format (kebab-case alphanumerics + hyphen).
    static bool IsValidThemeIdFormat(const std::wstring& themeId);

    /// Check package size constraint (max 10MB).
    static bool IsValidPackageSize(const std::wstring& packagePath);

    /// Validate no executable/script payloads in extracted archive content.
    static bool ContainsForbiddenContent(const std::wstring& extractedPath);
};

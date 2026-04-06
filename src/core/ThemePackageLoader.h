#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <windows.h>

class SettingsStore;

/// Loads and parses third-party theme packages (.zip archives).
/// Handles extraction, metadata parsing, and token loading.
class ThemePackageLoader
{
public:
    ThemePackageLoader() = default;
    ~ThemePackageLoader() = default;

    /// Theme metadata loaded from theme-metadata.json
    struct ThemeMetadata
    {
        std::wstring themeId;
        std::wstring displayName;
        std::wstring version;
        std::wstring author;
        std::wstring description;
        std::wstring defaultMode;  // "default" or specific mode (dark/light)

        bool IsValid() const
        {
            return !themeId.empty() && !displayName.empty() && !version.empty();
        }
    };

    /// Token map entries (token name → hex color string)
    struct TokenMap
    {
        std::unordered_map<std::wstring, std::wstring> tokens;

        bool GetToken(const std::wstring& name, std::wstring& valueOut) const
        {
            auto it = tokens.find(name);
            if (it != tokens.end())
            {
                valueOut = it->second;
                return true;
            }
            return false;
        }
    };

    /// Load result combining metadata and tokens
    struct LoadResult
    {
        bool success = false;
        std::wstring errorMessage;
        ThemeMetadata metadata;
        TokenMap tokenMap;
        std::wstring extractedPath;  // Temp extraction directory

        static LoadResult Failure(const std::wstring& error)
        {
            return {false, error, {}, {}, L""};
        }

        static LoadResult Success(const ThemeMetadata& meta, const TokenMap& tokens, const std::wstring& path)
        {
            return {true, L"", meta, tokens, path};
        }
    };

    /// Load a theme package from .zip file.
    /// Returns metadata, tokens, and temp extraction path on success.
    LoadResult LoadPackage(const std::wstring& packagePath) const;

    /// Parse metadata from theme-metadata.json content.
    ThemeMetadata ParseMetadata(const std::wstring& jsonContent) const;

    /// Load token map from theme tokens file (default.json, dark.json, or light.json).
    TokenMap LoadTokens(const std::wstring& tokensJsonPath) const;

    /// Clean up temporary extraction directory.
    static bool CleanupExtraction(const std::wstring& extractedPath);

private:
    /// Extract .zip archive to temporary directory.
    std::wstring ExtractPackage(const std::wstring& packagePath) const;

    /// Read file content from extracted archive.
    std::wstring ReadExtractedFile(const std::wstring& extractedPath, const std::wstring& relativePath) const;
};

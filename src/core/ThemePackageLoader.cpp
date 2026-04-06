#include "core/ThemePackageLoader.h"

#include "Win32Helpers.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <windows.h>
#include <shellapi.h>

namespace
{
    std::wstring EscapeForPowerShellSingleQuoted(const std::wstring& value)
    {
        std::wstring escaped;
        escaped.reserve(value.size());
        for (wchar_t ch : value)
        {
            escaped.push_back(ch);
            if (ch == L'\'')
            {
                escaped.push_back(L'\'');
            }
        }
        return escaped;
    }

    // Convert UTF-8 string to wide string
    std::wstring Utf8ToWString(const std::string& s)
    {
        if (s.empty())
            return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        if (size <= 0)
            return {};
        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), size);
        return result;
    }

    // Convert wide string to UTF-8
    std::string WStringToUtf8(const std::wstring& ws)
    {
        if (ws.empty())
            return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
            return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    // Generate unique temp directory for extraction
    std::wstring GetTempExtractionPath()
    {
        wchar_t tempDir[MAX_PATH] = {0};
        if (!GetTempPathW(MAX_PATH, tempDir))
            return L"";

        wchar_t uniquePath[MAX_PATH] = {0};
        if (!GetTempFileNameW(tempDir, L"THEME", 0, uniquePath))
            return L"";

        // Delete the file created by GetTempFileName and use it as a directory name
        DeleteFileW(uniquePath);
        std::filesystem::create_directories(uniquePath);
        return uniquePath;
    }

    // Wrapper for ZIP extraction using command line (portable approach)
    bool ExtractZipFile(const std::wstring& zipPath, const std::wstring& destPath)
    {
        // Use PowerShell to extract ZIP (available on Windows 7+ and doesn't require external libs)
        std::wstring command = L"powershell -NoProfile -Command \"Expand-Archive -LiteralPath '";
        command += EscapeForPowerShellSingleQuoted(zipPath);
        command += L"' -DestinationPath '";
        command += EscapeForPowerShellSingleQuoted(destPath);
        command += L"' -Force\"";

        STARTUPINFOW si = {sizeof(si)};
        PROCESS_INFORMATION pi = {};

        if (!CreateProcessW(nullptr, const_cast<wchar_t*>(command.c_str()), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return exitCode == 0;
    }

    // Validate hex color format (#RRGGBB)
    bool IsValidHexColor(const std::string& colorStr)
    {
        if (colorStr.length() != 7 || colorStr[0] != '#')
            return false;

        for (size_t i = 1; i < 7; ++i)
        {
            char c = colorStr[i];
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')))
                return false;
        }
        return true;
    }

    // Normalize token name to lowercase kebab-case
    std::wstring NormalizeTokenName(const std::wstring& name)
    {
        std::wstring result = name;
        for (auto& c : result)
        {
            if (c >= L'A' && c <= L'Z')
                c = L'a' + (c - L'A');
            else if (c == L'_')
                c = L'-';
        }
        return result;
    }
}

ThemePackageLoader::LoadResult ThemePackageLoader::LoadPackage(const std::wstring& packagePath) const
{
    if (packagePath.empty())
        return LoadResult::Failure(L"Package path is empty");

    if (!std::filesystem::exists(packagePath))
        return LoadResult::Failure(L"Package file does not exist: " + packagePath);

    // Extract package to temp directory
    std::wstring extractPath = ExtractPackage(packagePath);
    if (extractPath.empty())
        return LoadResult::Failure(L"Failed to extract theme package");

    try
    {
        // Read metadata file
        const std::filesystem::path extractedRoot(extractPath);
        std::filesystem::path metadataPath = extractedRoot / L"theme-metadata.json";
        if (!std::filesystem::exists(metadataPath))
        {
            CleanupExtraction(extractPath);
            return LoadResult::Failure(L"theme-metadata.json not found in package");
        }

        std::ifstream metadataFile(metadataPath);
        if (!metadataFile.is_open())
        {
            CleanupExtraction(extractPath);
            return LoadResult::Failure(L"Cannot read theme-metadata.json");
        }

        std::stringstream buffer;
        buffer << metadataFile.rdbuf();
        std::string jsonStr = buffer.str();

        ThemeMetadata metadata = ParseMetadata(Utf8ToWString(jsonStr));
        if (!metadata.IsValid())
        {
            CleanupExtraction(extractPath);
            return LoadResult::Failure(L"Invalid or incomplete theme metadata");
        }

        // Determine which token file to load (default, dark, or light)
        std::wstring tokenFileName = L"theme/tokens/default.json";
        if (metadata.defaultMode == L"dark")
            tokenFileName = L"theme/tokens/dark.json";
        else if (metadata.defaultMode == L"light")
            tokenFileName = L"theme/tokens/light.json";

        std::filesystem::path tokenPath = extractedRoot / tokenFileName;
        if (!std::filesystem::exists(tokenPath))
        {
            // Fall back to default.json if specified mode not found
            tokenPath = extractedRoot / L"theme/tokens/default.json";
            if (!std::filesystem::exists(tokenPath))
            {
                CleanupExtraction(extractPath);
                return LoadResult::Failure(L"No token file found in package");
            }
        }

        TokenMap tokens = LoadTokens(tokenPath.wstring());
        if (tokens.tokens.empty())
        {
            CleanupExtraction(extractPath);
            return LoadResult::Failure(L"Failed to load theme tokens");
        }

        return LoadResult::Success(metadata, tokens, extractPath);
    }
    catch (const std::exception& ex)
    {
        CleanupExtraction(extractPath);
        return LoadResult::Failure(L"Exception loading package: " + Utf8ToWString(ex.what()));
    }
}

ThemePackageLoader::ThemeMetadata ThemePackageLoader::ParseMetadata(const std::wstring& jsonContent) const
{
    ThemeMetadata result;

    try
    {
        std::string utf8Json = WStringToUtf8(jsonContent);
        nlohmann::json obj = nlohmann::json::parse(utf8Json);

        if (obj.contains("theme_id") && obj["theme_id"].is_string())
            result.themeId = Utf8ToWString(obj["theme_id"].get<std::string>());

        if (obj.contains("display_name") && obj["display_name"].is_string())
            result.displayName = Utf8ToWString(obj["display_name"].get<std::string>());

        if (obj.contains("version") && obj["version"].is_string())
            result.version = Utf8ToWString(obj["version"].get<std::string>());

        if (obj.contains("author") && obj["author"].is_string())
            result.author = Utf8ToWString(obj["author"].get<std::string>());

        if (obj.contains("description") && obj["description"].is_string())
            result.description = Utf8ToWString(obj["description"].get<std::string>());

        if (obj.contains("default_mode") && obj["default_mode"].is_string())
            result.defaultMode = Utf8ToWString(obj["default_mode"].get<std::string>());
        else
            result.defaultMode = L"default";
    }
    catch (const std::exception&)
    {
        // Return empty metadata on parse error
    }

    return result;
}

ThemePackageLoader::TokenMap ThemePackageLoader::LoadTokens(const std::wstring& tokensJsonPath) const
{
    TokenMap result;

    try
    {
        std::ifstream tokensFile(tokensJsonPath);
        if (!tokensFile.is_open())
            return result;

        nlohmann::json obj = nlohmann::json::parse(tokensFile);

        // Parse tokens from root object or from "tokens" key
        const nlohmann::json* tokenObj = &obj;
        if (obj.contains("tokens") && obj["tokens"].is_object())
            tokenObj = &obj["tokens"];

        for (auto it = tokenObj->begin(); it != tokenObj->end(); ++it)
        {
            if (it.value().is_string())
            {
                std::string tokenValue = it.value().get<std::string>();
                // Validate hex color format
                if (IsValidHexColor(tokenValue))
                {
                    std::wstring tokenName = NormalizeTokenName(Utf8ToWString(it.key()));
                    result.tokens[tokenName] = Utf8ToWString(tokenValue);
                }
            }
        }
    }
    catch (const std::exception&)
    {
        // Return empty map on parse error
    }

    return result;
}

bool ThemePackageLoader::CleanupExtraction(const std::wstring& extractedPath)
{
    try
    {
        if (std::filesystem::exists(extractedPath))
            std::filesystem::remove_all(extractedPath);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::wstring ThemePackageLoader::ExtractPackage(const std::wstring& packagePath) const
{
    std::wstring destPath = GetTempExtractionPath();
    if (destPath.empty())
        return L"";

    if (!ExtractZipFile(packagePath, destPath))
    {
        CleanupExtraction(destPath);
        return L"";
    }

    return destPath;
}

std::wstring ThemePackageLoader::ReadExtractedFile(const std::wstring& extractedPath, const std::wstring& relativePath) const
{
    try
    {
        std::filesystem::path filePath = extractedPath;
        filePath /= relativePath;

        if (!std::filesystem::exists(filePath))
            return L"";

        std::ifstream file(filePath);
        if (!file.is_open())
            return L"";

        std::stringstream buffer;
        buffer << file.rdbuf();
        return Utf8ToWString(buffer.str());
    }
    catch (const std::exception&)
    {
        return L"";
    }
}

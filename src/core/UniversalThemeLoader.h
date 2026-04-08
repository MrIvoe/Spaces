#pragma once

#include <string>
#include <unordered_map>
#include <windows.h>

struct UniversalThemeData
{
    std::string themeId;
    std::unordered_map<std::string, std::string> tokens;
    std::unordered_map<std::string, std::string> semantic;
};

class UniversalThemeLoader
{
public:
    static bool LoadFromDirectory(const std::wstring& themeDirectory, UniversalThemeData& outTheme, std::wstring* error = nullptr);
    static bool ResolveSemanticToken(const UniversalThemeData& theme, const std::string& semanticPath, std::string& outHexColor);
    static COLORREF ResolveSemanticColorRef(const UniversalThemeData& theme, const std::string& semanticPath, COLORREF fallback);
};

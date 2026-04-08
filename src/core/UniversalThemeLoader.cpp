#include "core/UniversalThemeLoader.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace
{
    std::wstring Utf8ToWString(const std::string& s)
    {
        if (s.empty())
        {
            return {};
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        if (size <= 0)
        {
            return {};
        }

        std::wstring out(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), size);
        return out;
    }

    std::string WStringToUtf8(const std::wstring& ws)
    {
        if (ws.empty())
        {
            return {};
        }

        const int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
        {
            return {};
        }

        std::string out(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), size, nullptr, nullptr);
        return out;
    }

    void FlattenObject(const nlohmann::json& value, const std::string& prefix, std::unordered_map<std::string, std::string>& out)
    {
        if (!value.is_object())
        {
            return;
        }

        for (auto it = value.begin(); it != value.end(); ++it)
        {
            const std::string nextKey = prefix.empty() ? it.key() : (prefix + "." + it.key());
            if (it->is_object())
            {
                FlattenObject(*it, nextKey, out);
                continue;
            }
            if (it->is_string())
            {
                out[nextKey] = it->get<std::string>();
            }
        }
    }

    COLORREF HexToColorRef(const std::string& hex)
    {
        if (hex.size() != 7 || hex[0] != '#')
        {
            return static_cast<COLORREF>(-1);
        }

        try
        {
            const int r = std::stoi(hex.substr(1, 2), nullptr, 16);
            const int g = std::stoi(hex.substr(3, 2), nullptr, 16);
            const int b = std::stoi(hex.substr(5, 2), nullptr, 16);
            return RGB(r, g, b);
        }
        catch (...)
        {
            return static_cast<COLORREF>(-1);
        }
    }
}

bool UniversalThemeLoader::LoadFromDirectory(const std::wstring& themeDirectory, UniversalThemeData& outTheme, std::wstring* error)
{
    const std::filesystem::path base(themeDirectory);
    const std::filesystem::path themePath = base / L"theme.json";
    const std::filesystem::path semanticPath = base / L"semantic.json";
    const std::filesystem::path iconsPath = base / L"icons.json";

    if (!std::filesystem::exists(themePath) || !std::filesystem::exists(semanticPath))
    {
        if (error)
        {
            *error = L"Missing theme.json or semantic.json in " + themeDirectory;
        }
        return false;
    }

    try
    {
        std::ifstream themeStream(themePath);
        std::ifstream semanticStream(semanticPath);
        if (!themeStream.good() || !semanticStream.good())
        {
            if (error)
            {
                *error = L"Unable to read theme files from " + themeDirectory;
            }
            return false;
        }

        nlohmann::json themeJson;
        nlohmann::json semanticJson;
        themeStream >> themeJson;
        semanticStream >> semanticJson;

        outTheme = {};
        outTheme.themeId = themeJson.value("meta", nlohmann::json::object()).value("id", WStringToUtf8(themeDirectory));

        if (themeJson.contains("tokens"))
        {
            FlattenObject(themeJson["tokens"], "", outTheme.tokens);
        }
        FlattenObject(semanticJson, "", outTheme.semantic);

        if (std::filesystem::exists(iconsPath))
        {
            std::ifstream iconsStream(iconsPath);
            if (iconsStream.good())
            {
                nlohmann::json iconsJson;
                iconsStream >> iconsJson;

                const auto iconIt = iconsJson.find("icon");
                if (iconIt != iconsJson.end() && iconIt->is_object())
                {
                    const auto packsIt = iconIt->find("packs");
                    if (packsIt != iconIt->end() && packsIt->is_object())
                    {
                        for (auto packIt = packsIt->begin(); packIt != packsIt->end(); ++packIt)
                        {
                            if (!packIt->is_object())
                            {
                                continue;
                            }

                            const auto mappingIt = packIt->find("mapping");
                            if (mappingIt == packIt->end() || !mappingIt->is_object())
                            {
                                continue;
                            }

                            auto& outMap = outTheme.iconPackMappings[packIt.key()];
                            for (auto iconMapIt = mappingIt->begin(); iconMapIt != mappingIt->end(); ++iconMapIt)
                            {
                                if (iconMapIt->is_string())
                                {
                                    outMap[iconMapIt.key()] = iconMapIt->get<std::string>();
                                }
                            }
                        }
                    }
                }
            }
        }

        if (outTheme.tokens.empty())
        {
            if (error)
            {
                *error = L"Theme does not define any tokens: " + themeDirectory;
            }
            return false;
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        if (error)
        {
            *error = L"Theme parse failed: " + Utf8ToWString(ex.what());
        }
        return false;
    }
}

bool UniversalThemeLoader::ResolveSemanticToken(const UniversalThemeData& theme, const std::string& semanticPath, std::string& outHexColor)
{
    const auto semanticIt = theme.semantic.find(semanticPath);
    if (semanticIt == theme.semantic.end())
    {
        return false;
    }

    const auto tokenIt = theme.tokens.find(semanticIt->second);
    if (tokenIt == theme.tokens.end())
    {
        return false;
    }

    outHexColor = tokenIt->second;
    return true;
}

COLORREF UniversalThemeLoader::ResolveSemanticColorRef(const UniversalThemeData& theme, const std::string& semanticPath, COLORREF fallback)
{
    std::string color;
    if (!ResolveSemanticToken(theme, semanticPath, color))
    {
        return fallback;
    }

    const COLORREF parsed = HexToColorRef(color);
    return parsed == static_cast<COLORREF>(-1) ? fallback : parsed;
}

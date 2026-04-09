#include "core/UniversalThemeLoader.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

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

    std::optional<std::string> JsonScalarToString(const nlohmann::json& value)
    {
        if (value.is_string())
        {
            return value.get<std::string>();
        }
        if (value.is_boolean())
        {
            return value.get<bool>() ? "true" : "false";
        }
        if (value.is_number_integer())
        {
            return std::to_string(value.get<long long>());
        }
        if (value.is_number_unsigned())
        {
            return std::to_string(value.get<unsigned long long>());
        }
        if (value.is_number_float())
        {
            std::ostringstream stream;
            stream << value.get<double>();
            return stream.str();
        }

        return std::nullopt;
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

            const std::optional<std::string> scalar = JsonScalarToString(*it);
            if (scalar.has_value())
            {
                out[nextKey] = *scalar;
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

    bool ResolveReferenceValue(const UniversalThemeData& theme, const std::string& rawValue, std::string& outValue)
    {
        const auto tokenIt = theme.tokens.find(rawValue);
        if (tokenIt != theme.tokens.end())
        {
            outValue = tokenIt->second;
            return true;
        }

        const auto scaleIt = theme.scale.find(rawValue);
        if (scaleIt != theme.scale.end())
        {
            outValue = scaleIt->second;
            return true;
        }

        const auto semanticIt = theme.semantic.find(rawValue);
        if (semanticIt != theme.semantic.end())
        {
            const auto semanticTokenIt = theme.tokens.find(semanticIt->second);
            if (semanticTokenIt != theme.tokens.end())
            {
                outValue = semanticTokenIt->second;
                return true;
            }
        }

        return false;
    }

    void FlattenResolvedComponents(const nlohmann::json& value,
                                   const std::string& prefix,
                                   const UniversalThemeData& theme,
                                   std::unordered_map<std::string, std::string>& out)
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
                FlattenResolvedComponents(*it, nextKey, theme, out);
                continue;
            }

            const std::optional<std::string> scalar = JsonScalarToString(*it);
            if (!scalar.has_value())
            {
                continue;
            }

            std::string resolvedValue;
            if (ResolveReferenceValue(theme, *scalar, resolvedValue))
            {
                out[nextKey] = resolvedValue;
                continue;
            }

            out[nextKey] = *scalar;
        }
    }
}

bool UniversalThemeLoader::LoadFromDirectory(const std::wstring& themeDirectory, UniversalThemeData& outTheme, std::wstring* error)
{
    const std::filesystem::path base(themeDirectory);
    const std::filesystem::path themePath = base / L"theme.json";
    const std::filesystem::path semanticPath = base / L"semantic.json";
    const std::filesystem::path componentsPath = base / L"components.json";
    const std::filesystem::path iconsPath = base / L"icons.json";
    const std::filesystem::path resourcesPath = base / L"resources.json";

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
        if (themeJson.contains("scale"))
        {
            FlattenObject(themeJson["scale"], "", outTheme.scale);

            const auto motionIt = themeJson["scale"].find("motion");
            if (motionIt != themeJson["scale"].end() && motionIt->is_object())
            {
                FlattenObject(*motionIt, "", outTheme.motion);
            }
        }
        FlattenObject(semanticJson, "", outTheme.semantic);

        if (std::filesystem::exists(componentsPath))
        {
            std::ifstream componentsStream(componentsPath);
            if (componentsStream.good())
            {
                nlohmann::json componentsJson;
                componentsStream >> componentsJson;
                FlattenResolvedComponents(componentsJson, "", outTheme, outTheme.components);
            }
        }

        if (std::filesystem::exists(resourcesPath))
        {
            std::ifstream resourcesStream(resourcesPath);
            if (resourcesStream.good())
            {
                nlohmann::json resourcesJson;
                resourcesStream >> resourcesJson;
                FlattenObject(resourcesJson, "", outTheme.resources);
            }
        }

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

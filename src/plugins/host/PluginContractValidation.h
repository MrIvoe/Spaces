#pragma once

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

#include "plugins/host/PluginContractModels.h"
#include "plugins/host/ThemeTokenPolicy.h"

namespace HostPlugins
{
    inline std::wstring ToLowerCopy(const std::wstring& value)
    {
        std::wstring lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
        return lowered;
    }

    inline bool IsHexString(const std::wstring& value)
    {
        for (wchar_t ch : value)
        {
            if (!std::iswxdigit(ch))
                return false;
        }
        return true;
    }

    inline bool IsChannelAllowed(const std::wstring& requestedChannel, const std::wstring& hostPolicyChannel)
    {
        const std::wstring requested = ToLowerCopy(requestedChannel);
        const std::wstring policy = ToLowerCopy(hostPolicyChannel);

        if (requested != L"stable" && requested != L"preview")
            return false;

        if (policy == L"stable")
            return requested == L"stable";

        if (policy == L"preview")
            return requested == L"stable" || requested == L"preview";

        return false;
    }

    inline ValidationResult ValidateHostedSummaryPanel(const HostedSummaryPanel& panel)
    {
        if (!IsValidThemeTokenNamespace(panel.themeTokenNamespace))
            return ValidationResult::Failure(L"hosted summary panel has invalid theme token namespace");

        if (panel.sections.empty())
            return ValidationResult::Failure(L"hosted summary panel requires at least one section");

        for (const auto& section : panel.sections)
        {
            if (section.sectionId.empty() || section.title.empty())
                return ValidationResult::Failure(L"hosted summary section requires id and title");

            if (!IsValidThemeTokenPath(section.iconToken) || !IsValidThemeTokenPath(section.surfaceToken))
                return ValidationResult::Failure(L"hosted summary section has invalid token paths");
        }

        return ValidationResult::Success();
    }

    inline ValidationResult ValidateManifestContract(const PluginManifestContract& manifest)
    {
        if (manifest.id.empty() || manifest.displayName.empty() || manifest.version.empty())
            return ValidationResult::Failure(L"manifest id, display name, and version are required");

        if (!IsChannelAllowed(manifest.updateChannelId, L"preview"))
            return ValidationResult::Failure(L"manifest update channel is invalid");

        const bool hasSettingsCapability = std::find(manifest.capabilities.begin(), manifest.capabilities.end(), L"settings_pages") != manifest.capabilities.end();
        if (hasSettingsCapability && !manifest.supportsSettingsPage)
            return ValidationResult::Failure(L"settings_pages capability requires supportsSettingsPage=true");

        const bool hasWidgetsCapability = std::find(manifest.capabilities.begin(), manifest.capabilities.end(), L"widgets") != manifest.capabilities.end();
        if (hasWidgetsCapability && !manifest.supportsMainContentPage)
            return ValidationResult::Failure(L"widgets capability requires supportsMainContentPage=true");

        if (!manifest.supportsHostedSummaryPanel)
            return ValidationResult::Failure(L"manifest must explicitly support hosted summary panel");

        return ValidateHostedSummaryPanel(manifest.hostedSummaryPanel);
    }

    inline ValidationResult ValidateUpdateFeedEntry(const UpdateFeedEntry& entry)
    {
        if (entry.pluginId.empty() || entry.version.empty() || entry.packageUrl.empty())
            return ValidationResult::Failure(L"update feed entry is missing required fields");

        if (!IsChannelAllowed(entry.updateChannelId, L"preview"))
            return ValidationResult::Failure(L"update feed entry channel is invalid");

        if (entry.signature.algorithm != L"ecdsa-p256-sha256")
            return ValidationResult::Failure(L"unsupported signature algorithm");

        if (entry.sha256.size() != 64 || !IsHexString(entry.sha256))
            return ValidationResult::Failure(L"invalid SHA-256 hash format");

        if (entry.signature.signatureBase64.empty())
            return ValidationResult::Failure(L"signature payload is missing");

        return ValidationResult::Success();
    }
}

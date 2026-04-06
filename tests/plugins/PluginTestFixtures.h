#pragma once

#include <fstream>
#include <string>

#include <windows.h>

#include "plugins/host/PluginContractModels.h"

namespace HostPlugins::TestFixtures
{
    inline PluginManifestContract MakeValidManifest()
    {
        PluginManifestContract m;
        m.id = L"com.ivoe.sample.grid";
        m.version = L"1.2.3";
        m.displayName = L"Sample Grid";
        m.author = L"IVOE";
        m.description = L"Sample hosted plugin";
        m.updateChannelId = L"stable";
        m.supportsHostedSummaryPanel = true;
        m.supportsSettingsPage = true;
        m.supportsMainContentPage = true;
        m.capabilities = {L"settings_pages", L"widgets"};

        m.minHostVersion = L"1.0.0";
        m.maxHostVersion = L"2.0.0";
        m.minHostApiVersion = L"1.0.0";
        m.maxHostApiVersion = L"2.0.0";

        m.hostedSummaryPanel.panelId = L"summary.main";
        m.hostedSummaryPanel.schemaVersion = L"1.0";
        m.hostedSummaryPanel.layout = L"list";
        m.hostedSummaryPanel.themeTokenNamespace = L"win32_theme_system";
        m.hostedSummaryPanel.title = L"Plugin Summary";
        m.hostedSummaryPanel.sections.push_back(HostedSummarySection{
            L"status",
            L"Status",
            L"Ready",
            L"host.icons.info",
            L"theme.surface.default"
        });
        return m;
    }

    inline UpdateFeedEntry MakeValidFeedEntry()
    {
        UpdateFeedEntry e;
        e.pluginId = L"com.ivoe.sample.grid";
        e.version = L"1.2.4";
        e.updateChannelId = L"stable";
        e.packageUrl = L"https://example.invalid/package.ivoepkg";
        e.sha256 = L"0000000000000000000000000000000000000000000000000000000000000000";

        e.minHostVersion = L"1.0.0";
        e.maxHostVersion = L"2.0.0";
        e.minHostApiVersion = L"1.0.0";
        e.maxHostApiVersion = L"2.0.0";

        e.signature.algorithm = L"ecdsa-p256-sha256";
        e.signature.signatureBase64 = L"MEQCIFakeSignature==";
        e.signature.signingCertChainPem = {
            L"-----BEGIN CERTIFICATE-----\nFAKE\n-----END CERTIFICATE-----"
        };
        e.signature.leafThumbprintSha256 = L"1111111111111111111111111111111111111111111111111111111111111111";
        return e;
    }

    inline HostVersionContext MakeHostContext()
    {
        HostVersionContext h;
        h.hostVersion = L"1.5.0";
        h.hostApiVersion = L"1.3.0";
        h.updatePolicyChannel = L"stable";
        return h;
    }

    inline std::wstring WriteTempFileWithContent(const std::wstring& fileName, const std::string& content)
    {
        wchar_t tempPath[MAX_PATH] = { 0 };
        GetTempPathW(MAX_PATH, tempPath);

        std::wstring fullPath = std::wstring(tempPath) + fileName;
        std::ofstream out(fullPath, std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.close();
        return fullPath;
    }

    inline void RemoveFileIfExists(const std::wstring& path)
    {
        DeleteFileW(path.c_str());
    }
}

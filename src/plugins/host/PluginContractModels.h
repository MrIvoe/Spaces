#pragma once

#include <string>
#include <vector>

namespace HostPlugins
{
    struct ValidationResult
    {
        bool ok = false;
        std::wstring message;

        static ValidationResult Success()
        {
            return {true, L"ok"};
        }

        static ValidationResult Failure(const std::wstring& msg)
        {
            return {false, msg};
        }
    };

    struct HostedSummarySection
    {
        std::wstring sectionId;
        std::wstring title;
        std::wstring description;
        std::wstring iconToken;
        std::wstring surfaceToken;
    };

    struct HostedSummaryPanel
    {
        std::wstring panelId;
        std::wstring title;
        std::wstring schemaVersion;
        std::wstring layout;
        std::wstring themeTokenNamespace;
        std::vector<HostedSummarySection> sections;
    };

    struct SignatureContract
    {
        std::wstring algorithm;
        std::wstring signatureBase64;
        std::vector<std::wstring> signingCertChainPem;
        std::wstring leafThumbprintSha256;
    };

    struct PluginManifestContract
    {
        std::wstring id;
        std::wstring displayName;
        std::wstring version;
        std::wstring description;
        std::wstring author;

        std::wstring minHostVersion;
        std::wstring maxHostVersion;
        std::wstring minHostApiVersion;
        std::wstring maxHostApiVersion;

        bool enabledByDefault = true;
        bool supportsSettingsPage = false;
        bool supportsMainContentPage = false;
        bool supportsHostedSummaryPanel = false;

        std::wstring icon;
        std::wstring updateChannelId;
        std::vector<std::wstring> capabilities;
        std::wstring repository;

        HostedSummaryPanel hostedSummaryPanel;
    };

    struct UpdateFeedEntry
    {
        std::wstring pluginId;
        std::wstring displayName;
        std::wstring version;
        std::wstring author;
        std::wstring description;
        std::wstring minHostVersion;
        std::wstring maxHostVersion;
        std::wstring minHostApiVersion;
        std::wstring maxHostApiVersion;
        std::wstring packageUrl;
        std::wstring sha256;
        long long packageSizeBytes = 0;
        std::wstring updateChannelId;
        SignatureContract signature;
        std::wstring releaseNotesUrl;
    };

    struct HostVersionContext
    {
        std::wstring hostVersion;
        std::wstring hostApiVersion;
        std::wstring updatePolicyChannel;
        std::wstring trustedRootStoreName;
    };
}

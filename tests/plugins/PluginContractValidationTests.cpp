#include "PluginTestFixtures.h"

#include <string>

#include "plugins/host/PluginContractValidation.h"

namespace
{
    int Fail(const char* message)
    {
        fprintf(stderr, "[FAIL] %s\n", message);
        return 1;
    }
}

int RunPluginContractValidationTests()
{
    using namespace HostPlugins;
    using namespace HostPlugins::TestFixtures;

    {
        PluginManifestContract manifest = MakeValidManifest();
        const ValidationResult result = ValidateManifestContract(manifest);
        if (!result.ok)
            return Fail("ValidateManifestContract rejected valid manifest");
    }

    {
        PluginManifestContract manifest = MakeValidManifest();
        manifest.id = L"";
        const ValidationResult result = ValidateManifestContract(manifest);
        if (result.ok)
            return Fail("ValidateManifestContract accepted invalid plugin id");
    }

    {
        PluginManifestContract manifest = MakeValidManifest();
        manifest.hostedSummaryPanel.sections.push_back(HostedSummarySection{L"", L"value"});
        const ValidationResult result = ValidateManifestContract(manifest);
        if (result.ok)
            return Fail("ValidateManifestContract accepted invalid hosted summary section");
    }

    {
        UpdateFeedEntry entry = MakeValidFeedEntry();
        const ValidationResult result = ValidateUpdateFeedEntry(entry);
        if (!result.ok)
            return Fail("ValidateUpdateFeedEntry rejected valid entry");
    }

    {
        UpdateFeedEntry entry = MakeValidFeedEntry();
        entry.sha256 = L"xyz";
        const ValidationResult result = ValidateUpdateFeedEntry(entry);
        if (result.ok)
            return Fail("ValidateUpdateFeedEntry accepted non-hex SHA-256");
    }

    {
        UpdateFeedEntry entry = MakeValidFeedEntry();
        entry.updateChannelId = L"preview";
        const bool allowedForStable = IsChannelAllowed(entry.updateChannelId, L"stable");
        if (allowedForStable)
            return Fail("stable host should block preview update");

        const bool allowedForPreview = IsChannelAllowed(entry.updateChannelId, L"preview");
        if (!allowedForPreview)
            return Fail("preview host should allow preview update");
    }

    return 0;
}

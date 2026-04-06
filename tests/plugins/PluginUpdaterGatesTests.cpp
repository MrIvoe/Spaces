#include "PluginTestFixtures.h"

#include <string>

#include "plugins/host/PluginUpdaterGates.h"

namespace
{
    int Fail(const char* message)
    {
        fprintf(stderr, "[FAIL] %s\n", message);
        return 1;
    }
}

int RunPluginUpdaterGatesTests()
{
    using namespace HostPlugins;
    using namespace HostPlugins::TestFixtures;

    {
        PluginManifestContract manifest = MakeValidManifest();
        HostVersionContext host = MakeHostContext();
        const ValidationResult result = InstallGate(manifest, host);
        if (!result.ok)
            return Fail("InstallGate rejected valid manifest/host pair");
    }

    {
        PluginManifestContract manifest = MakeValidManifest();
        HostVersionContext host = MakeHostContext();
        host.updatePolicyChannel = L"stable";
        manifest.updateChannelId = L"preview";
        const ValidationResult result = InstallGate(manifest, host);
        if (result.ok)
            return Fail("InstallGate accepted manifest from blocked channel");
    }

    {
        PluginManifestContract manifest = MakeValidManifest();
        HostVersionContext host = MakeHostContext();
        manifest.minHostVersion = L"2.0.0";
        manifest.maxHostVersion = L"3.0.0";
        const ValidationResult result = ActivateGate(manifest, host);
        if (result.ok)
            return Fail("ActivateGate accepted incompatible host version");
    }

    {
        HostVersionContext host = MakeHostContext();
        UpdateFeedEntry entry = MakeValidFeedEntry();

        const std::wstring file = WriteTempFileWithContent(L"ivoe-plugin-stage-success.bin", "abc");
        entry.sha256 = L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

        const ValidationResult result = StageGate(entry, file, host);
        RemoveFileIfExists(file);
        if (!result.ok)
            return Fail("StageGate rejected valid feed/package/host");
    }

    {
        HostVersionContext host = MakeHostContext();
        UpdateFeedEntry entry = MakeValidFeedEntry();

        const std::wstring file = WriteTempFileWithContent(L"ivoe-plugin-stage-hash-fail.bin", "abc");
        entry.sha256 = L"0000000000000000000000000000000000000000000000000000000000000000";

        const ValidationResult result = StageGate(entry, file, host);
        RemoveFileIfExists(file);
        if (result.ok)
            return Fail("StageGate accepted mismatched package hash");
    }

    {
        HostVersionContext host = MakeHostContext();
        host.updatePolicyChannel = L"stable";
        UpdateFeedEntry entry = MakeValidFeedEntry();
        entry.updateChannelId = L"preview";

        const std::wstring file = WriteTempFileWithContent(L"ivoe-plugin-stage-channel-fail.bin", "abc");
        entry.sha256 = L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

        const ValidationResult result = StageGate(entry, file, host);
        RemoveFileIfExists(file);
        if (result.ok)
            return Fail("StageGate accepted blocked update channel");
    }

    return 0;
}

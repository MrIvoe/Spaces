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

int RunPluginSecurityPolicyTests()
{
    using namespace HostPlugins;
    using namespace HostPlugins::TestFixtures;

    {
        HostVersionContext host = MakeHostContext();
        UpdateFeedEntry entry = MakeValidFeedEntry();
        const ValidationResult sig = VerifySignaturePolicy(entry, host);
        if (!sig.ok)
            return Fail("VerifySignaturePolicy rejected valid signature contract");
    }

    {
        HostVersionContext host = MakeHostContext();
        UpdateFeedEntry entry = MakeValidFeedEntry();
        entry.signature.algorithm = L"rsa-pss";
        const ValidationResult sig = VerifySignaturePolicy(entry, host);
        if (sig.ok)
            return Fail("VerifySignaturePolicy accepted disallowed algorithm");
    }

    {
        HostVersionContext host = MakeHostContext();
        UpdateFeedEntry entry = MakeValidFeedEntry();
        entry.signature.leafThumbprintSha256 = L"abcd";
        const ValidationResult sig = VerifySignaturePolicy(entry, host);
        if (sig.ok)
            return Fail("VerifySignaturePolicy accepted short thumbprint");
    }

    {
        std::wstring hash;
        const std::wstring file = WriteTempFileWithContent(L"ivoe-plugin-hash-test.bin", "abc");
        const ValidationResult result = ComputeSha256File(file, hash);
        RemoveFileIfExists(file);

        if (!result.ok)
            return Fail("ComputeSha256File failed for existing file");

        if (hash != L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
            return Fail("ComputeSha256File produced unexpected digest for 'abc'");
    }

    return 0;
}

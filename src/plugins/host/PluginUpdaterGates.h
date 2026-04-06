#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#include "plugins/host/PluginContractModels.h"
#include "plugins/host/PluginContractValidation.h"

namespace HostPlugins
{
    struct ParsedVersion
    {
        int major = 0;
        int minor = 0;
        int patch = 0;
    };

    inline bool TryParseVersion(const std::wstring& text, ParsedVersion& out)
    {
        out = {};
        int consumed = swscanf_s(text.c_str(), L"%d.%d.%d", &out.major, &out.minor, &out.patch);
        return consumed == 3;
    }

    inline int CompareVersion(const ParsedVersion& a, const ParsedVersion& b)
    {
        if (a.major != b.major) return a.major < b.major ? -1 : 1;
        if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
        if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
        return 0;
    }

    inline bool VersionInRange(const std::wstring& current, const std::wstring& min, const std::wstring& max)
    {
        ParsedVersion currentV;
        ParsedVersion minV;
        ParsedVersion maxV;
        if (!TryParseVersion(current, currentV) || !TryParseVersion(min, minV) || !TryParseVersion(max, maxV))
            return false;

        return CompareVersion(currentV, minV) >= 0 && CompareVersion(currentV, maxV) <= 0;
    }

    inline ValidationResult VerifySignaturePolicy(const UpdateFeedEntry& entry, const HostVersionContext& host)
    {
        (void)host;

        if (entry.signature.algorithm != L"ecdsa-p256-sha256")
            return ValidationResult::Failure(L"signature algorithm is not allowed");

        if (entry.signature.signatureBase64.empty())
            return ValidationResult::Failure(L"signature payload is missing");

        if (entry.signature.signingCertChainPem.empty())
            return ValidationResult::Failure(L"signing certificate chain is missing");

        if (entry.signature.leafThumbprintSha256.size() != 64 || !IsHexString(entry.signature.leafThumbprintSha256))
            return ValidationResult::Failure(L"leaf certificate thumbprint must be SHA-256 hex");

        return ValidationResult::Success();
    }

    inline ValidationResult ComputeSha256File(const std::wstring& path, std::wstring& outHashHex)
    {
        outHashHex.clear();

        BCRYPT_ALG_HANDLE alg = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        PUCHAR objectBuffer = nullptr;
        PUCHAR hashBuffer = nullptr;

        DWORD objectLength = 0;
        DWORD dataLength = 0;
        DWORD hashLength = 0;
        NTSTATUS status = 0;

        std::ifstream in(path, std::ios::binary);
        if (!in)
            return ValidationResult::Failure(L"package file was not found");

        status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        if (status < 0)
            return ValidationResult::Failure(L"failed to open SHA-256 algorithm provider");

        status = BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &dataLength, 0);
        if (status < 0)
        {
            BCryptCloseAlgorithmProvider(alg, 0);
            return ValidationResult::Failure(L"failed to get hash object length");
        }

        status = BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength), &dataLength, 0);
        if (status < 0)
        {
            BCryptCloseAlgorithmProvider(alg, 0);
            return ValidationResult::Failure(L"failed to get hash length");
        }

        objectBuffer = static_cast<PUCHAR>(HeapAlloc(GetProcessHeap(), 0, objectLength));
        hashBuffer = static_cast<PUCHAR>(HeapAlloc(GetProcessHeap(), 0, hashLength));
        if (!objectBuffer || !hashBuffer)
        {
            if (objectBuffer) HeapFree(GetProcessHeap(), 0, objectBuffer);
            if (hashBuffer) HeapFree(GetProcessHeap(), 0, hashBuffer);
            BCryptCloseAlgorithmProvider(alg, 0);
            return ValidationResult::Failure(L"failed to allocate hash buffers");
        }

        status = BCryptCreateHash(alg, &hash, objectBuffer, objectLength, nullptr, 0, 0);
        if (status < 0)
        {
            HeapFree(GetProcessHeap(), 0, objectBuffer);
            HeapFree(GetProcessHeap(), 0, hashBuffer);
            BCryptCloseAlgorithmProvider(alg, 0);
            return ValidationResult::Failure(L"failed to create hash object");
        }

        constexpr size_t kChunk = 64 * 1024;
        std::string buffer(kChunk, '\0');
        while (in)
        {
            in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize got = in.gcount();
            if (got <= 0)
                break;

            status = BCryptHashData(hash,
                reinterpret_cast<PUCHAR>(buffer.data()),
                static_cast<ULONG>(got),
                0);
            if (status < 0)
            {
                BCryptDestroyHash(hash);
                HeapFree(GetProcessHeap(), 0, objectBuffer);
                HeapFree(GetProcessHeap(), 0, hashBuffer);
                BCryptCloseAlgorithmProvider(alg, 0);
                return ValidationResult::Failure(L"failed while hashing package data");
            }
        }

        status = BCryptFinishHash(hash, hashBuffer, hashLength, 0);
        if (status < 0)
        {
            BCryptDestroyHash(hash);
            HeapFree(GetProcessHeap(), 0, objectBuffer);
            HeapFree(GetProcessHeap(), 0, hashBuffer);
            BCryptCloseAlgorithmProvider(alg, 0);
            return ValidationResult::Failure(L"failed to finalize package hash");
        }

        static const wchar_t* kHex = L"0123456789abcdef";
        outHashHex.reserve(hashLength * 2);
        for (DWORD i = 0; i < hashLength; ++i)
        {
            unsigned char b = hashBuffer[i];
            outHashHex.push_back(kHex[(b >> 4) & 0x0F]);
            outHashHex.push_back(kHex[b & 0x0F]);
        }

        BCryptDestroyHash(hash);
        HeapFree(GetProcessHeap(), 0, objectBuffer);
        HeapFree(GetProcessHeap(), 0, hashBuffer);
        BCryptCloseAlgorithmProvider(alg, 0);
        return ValidationResult::Success();
    }

    inline ValidationResult InstallGate(const PluginManifestContract& manifest, const HostVersionContext& host)
    {
        const ValidationResult contract = ValidateManifestContract(manifest);
        if (!contract.ok)
            return contract;

        if (!IsChannelAllowed(manifest.updateChannelId, host.updatePolicyChannel))
            return ValidationResult::Failure(L"manifest channel is blocked by host policy");

        if (!VersionInRange(host.hostVersion, manifest.minHostVersion, manifest.maxHostVersion))
            return ValidationResult::Failure(L"host version is outside plugin manifest range");

        if (!VersionInRange(host.hostApiVersion, manifest.minHostApiVersion, manifest.maxHostApiVersion))
            return ValidationResult::Failure(L"host API version is outside plugin manifest range");

        return ValidationResult::Success();
    }

    inline ValidationResult ActivateGate(const PluginManifestContract& manifest, const HostVersionContext& host)
    {
        return InstallGate(manifest, host);
    }

    inline ValidationResult StageGate(const UpdateFeedEntry& entry, const std::wstring& packagePath, const HostVersionContext& host)
    {
        const ValidationResult feed = ValidateUpdateFeedEntry(entry);
        if (!feed.ok)
            return feed;

        if (!IsChannelAllowed(entry.updateChannelId, host.updatePolicyChannel))
            return ValidationResult::Failure(L"update channel is blocked by host policy");

        const ValidationResult sig = VerifySignaturePolicy(entry, host);
        if (!sig.ok)
            return sig;

        std::wstring actualHash;
        const ValidationResult hashResult = ComputeSha256File(packagePath, actualHash);
        if (!hashResult.ok)
            return hashResult;

        if (ToLowerCopy(actualHash) != ToLowerCopy(entry.sha256))
            return ValidationResult::Failure(L"package hash mismatch");

        if (!VersionInRange(host.hostVersion, entry.minHostVersion, entry.maxHostVersion))
            return ValidationResult::Failure(L"host version is outside update range");

        if (!VersionInRange(host.hostApiVersion, entry.minHostApiVersion, entry.maxHostApiVersion))
            return ValidationResult::Failure(L"host API version is outside update range");

        return ValidationResult::Success();
    }
}

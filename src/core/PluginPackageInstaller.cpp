#include "core/PluginPackageInstaller.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <random>

#include <nlohmann/json.hpp>

#include <windows.h>
#include <wininet.h>
#include <wincrypt.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace PluginPackage
{
    namespace
    {
        std::wstring ToLowerCopy(std::wstring text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
            return text;
        }

        bool RunProcess(const std::wstring& appPath,
                        const std::wstring& commandLine,
                        const fs::path& cwd,
                        DWORD& exitCode)
        {
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};

            std::vector<wchar_t> cmd(commandLine.begin(), commandLine.end());
            cmd.push_back(L'\0');

            std::wstring cwdText = cwd.wstring();
            const BOOL started = CreateProcessW(
                appPath.c_str(),
                cmd.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_NO_WINDOW,
                nullptr,
                cwdText.empty() ? nullptr : cwdText.c_str(),
                &si,
                &pi);

            if (!started)
            {
                exitCode = GetLastError();
                return false;
            }

            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return true;
        }

        std::wstring QuotePath(const fs::path& path)
        {
            return L"\"" + path.wstring() + L"\"";
        }

        std::wstring Utf8ToWide(const std::string& s)
        {
            return std::wstring(s.begin(), s.end());
        }
    }

    PackageInstaller::PackageInstaller(const fs::path& pluginBasePath)
        : m_pluginBasePath(pluginBasePath)
        , m_backupRootPath(pluginBasePath / L"_backup")
    {
        std::error_code ec;
        fs::create_directories(m_pluginBasePath, ec);
        fs::create_directories(m_backupRootPath, ec);
    }

    PackageInstaller::~PackageInstaller() = default;

    InstallStatus PackageInstaller::InstallFromUrl(
        const std::wstring& pluginId,
        const std::wstring& downloadUrl,
        const std::wstring& expectedSha256)
    {
        const fs::path tempDir = BuildTempPath(L"download", pluginId);
        std::error_code ec;
        fs::create_directories(tempDir, ec);

        const fs::path zipPath = tempDir / (pluginId + L".zip");

        if (!DownloadFile(downloadUrl, zipPath))
        {
            m_lastError = L"Failed to download plugin from: " + downloadUrl;
            fs::remove_all(tempDir, ec);
            return InstallStatus::DownloadFailed;
        }

        if (!expectedSha256.empty() && !VerifyPackageIntegrity(zipPath, expectedSha256))
        {
            fs::remove_all(tempDir, ec);
            return InstallStatus::ValidationFailed;
        }

        const InstallStatus status = InstallFromZip(pluginId, zipPath);
        fs::remove_all(tempDir, ec);
        return status;
    }

    InstallStatus PackageInstaller::InstallFromZip(
        const std::wstring& pluginId,
        const fs::path& zipPath)
    {
        if (!fs::exists(zipPath))
        {
            m_lastError = L"Plugin zip file not found: " + zipPath.wstring();
            return InstallStatus::InvalidPackage;
        }

        const fs::path targetPluginDir = m_pluginBasePath / pluginId;

        if (fs::exists(targetPluginDir))
        {
            m_lastError = L"Plugin is already installed. Please uninstall first to reinstall.";
            return InstallStatus::AlreadyInstalled;
        }

        std::error_code ec;
        const fs::path extractDir = BuildTempPath(L"extract", pluginId);
        fs::remove_all(extractDir, ec);
        fs::create_directories(extractDir, ec);

        if (!ExtractZip(zipPath, extractDir))
        {
            m_lastError = L"Failed to extract plugin package.";
            fs::remove_all(extractDir, ec);
            return InstallStatus::ExtractionFailed;
        }

        const fs::path pluginRoot = ResolveExtractedPluginRoot(extractDir);
        if (pluginRoot.empty())
        {
            m_lastError = L"Extracted package did not contain a plugin root with manifest.json.";
            fs::remove_all(extractDir, ec);
            return InstallStatus::ValidationFailed;
        }

        if (!ValidatePluginManifest(pluginRoot, pluginId))
        {
            fs::remove_all(extractDir, ec);
            return InstallStatus::ValidationFailed;
        }

        if (!MoveDirectory(pluginRoot, targetPluginDir))
        {
            m_lastError = L"Failed to move plugin files during installation.";
            fs::remove_all(extractDir, ec);
            fs::remove_all(targetPluginDir, ec);
            return InstallStatus::InstallFailed;
        }

        fs::remove_all(extractDir, ec);
        return InstallStatus::Success;
    }

    InstallStatus PackageInstaller::UpdateFromUrl(
        const std::wstring& pluginId,
        const std::wstring& downloadUrl,
        const std::wstring& expectedSha256,
        bool keepBackup)
    {
        const fs::path tempDir = BuildTempPath(L"download", pluginId);
        std::error_code ec;
        fs::create_directories(tempDir, ec);

        const fs::path zipPath = tempDir / (pluginId + L".zip");
        if (!DownloadFile(downloadUrl, zipPath))
        {
            m_lastError = L"Failed to download plugin update from: " + downloadUrl;
            fs::remove_all(tempDir, ec);
            return InstallStatus::DownloadFailed;
        }

        if (!expectedSha256.empty() && !VerifyPackageIntegrity(zipPath, expectedSha256))
        {
            fs::remove_all(tempDir, ec);
            return InstallStatus::ValidationFailed;
        }

        const InstallStatus status = UpdateFromZip(pluginId, zipPath, keepBackup);
        fs::remove_all(tempDir, ec);
        return status;
    }

    InstallStatus PackageInstaller::UpdateFromZip(
        const std::wstring& pluginId,
        const fs::path& zipPath,
        bool keepBackup)
    {
        if (!fs::exists(zipPath))
        {
            m_lastError = L"Plugin zip file not found: " + zipPath.wstring();
            return InstallStatus::InvalidPackage;
        }

        const fs::path targetPluginDir = m_pluginBasePath / pluginId;
        if (!fs::exists(targetPluginDir))
        {
            // No existing install - treat update as install.
            return InstallFromZip(pluginId, zipPath);
        }

        std::error_code ec;
        const fs::path extractDir = BuildTempPath(L"extract", pluginId);
        fs::remove_all(extractDir, ec);
        fs::create_directories(extractDir, ec);

        if (!ExtractZip(zipPath, extractDir))
        {
            m_lastError = L"Failed to extract plugin package for update.";
            fs::remove_all(extractDir, ec);
            return InstallStatus::ExtractionFailed;
        }

        const fs::path pluginRoot = ResolveExtractedPluginRoot(extractDir);
        if (pluginRoot.empty() || !ValidatePluginManifest(pluginRoot, pluginId))
        {
            if (m_lastError.empty())
            {
                m_lastError = L"Updated package manifest validation failed.";
            }
            fs::remove_all(extractDir, ec);
            return InstallStatus::ValidationFailed;
        }

        if (!ReplacePluginWithRollback(targetPluginDir, pluginRoot, keepBackup))
        {
            fs::remove_all(extractDir, ec);
            return InstallStatus::InstallFailed;
        }

        fs::remove_all(extractDir, ec);
        return InstallStatus::Success;
    }

    bool PackageInstaller::Uninstall(const std::wstring& pluginId)
    {
        const fs::path pluginDir = m_pluginBasePath / pluginId;

        if (!fs::exists(pluginDir))
        {
            m_lastError = L"Plugin not found: " + pluginId;
            return false;
        }

        std::error_code ec;
        fs::remove_all(pluginDir, ec);

        if (ec)
        {
            m_lastError = L"Failed to uninstall plugin: " + pluginId;
            return false;
        }

        return true;
    }

    bool PackageInstaller::DownloadFile(const std::wstring& url, const fs::path& saveTo)
    {
        HINTERNET hInternet = InternetOpenW(
            L"Spaces/1.01.001",
            INTERNET_OPEN_TYPE_PRECONFIG,
            nullptr,
            nullptr,
            0);

        if (!hInternet)
        {
            return false;
        }

        HINTERNET hUrl = InternetOpenUrlW(
            hInternet,
            url.c_str(),
            nullptr,
            0,
            INTERNET_FLAG_RELOAD,
            0);

        if (!hUrl)
        {
            InternetCloseHandle(hInternet);
            return false;
        }

        std::ofstream outFile(saveTo, std::ios::binary);
        if (!outFile)
        {
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return false;
        }

        const DWORD readBufferSize = 8192;
        std::vector<char> readBuffer(readBufferSize);
        DWORD bytesRead = 0;

        while (InternetReadFile(hUrl, readBuffer.data(), readBufferSize, &bytesRead))
        {
            if (bytesRead == 0)
                break;
            outFile.write(readBuffer.data(), bytesRead);
        }

        outFile.close();
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        return fs::exists(saveTo) && fs::file_size(saveTo) > 0;
    }

    bool PackageInstaller::ExtractZip(const fs::path& zipPath, const fs::path& targetDir)
    {
        std::error_code ec;
        fs::create_directories(targetDir, ec);

        // Use Windows tar.exe (bsdtar) to extract .zip archives.
        DWORD exitCode = 0;
        const std::wstring args =
            L"tar.exe -xf " + QuotePath(zipPath) + L" -C " + QuotePath(targetDir);
        if (!RunProcess(L"C:\\Windows\\System32\\cmd.exe", L"cmd.exe /c " + args, targetDir, exitCode) || exitCode != 0)
        {
            m_lastError = L"ZIP extraction failed (tar exit code " + std::to_wstring(exitCode) + L").";
            return false;
        }

        // Basic extraction safety validation: ensure extracted entries remain under target root.
        const fs::path canonicalRoot = fs::weakly_canonical(targetDir, ec);
        if (ec)
        {
            m_lastError = L"Failed to canonicalize extraction directory.";
            return false;
        }

        for (const auto& entry : fs::recursive_directory_iterator(targetDir, ec))
        {
            if (ec)
            {
                m_lastError = L"Failed while traversing extracted package.";
                return false;
            }

            const fs::path canonicalEntry = fs::weakly_canonical(entry.path(), ec);
            if (ec)
            {
                continue;
            }

            const std::wstring rootText = canonicalRoot.wstring();
            const std::wstring entryText = canonicalEntry.wstring();
            if (entryText.size() < rootText.size() || entryText.compare(0, rootText.size(), rootText) != 0)
            {
                m_lastError = L"Unsafe path detected in extracted package.";
                return false;
            }
        }

        return true;
    }

    std::wstring PackageInstaller::CalculateFileSha256(const fs::path& filePath)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            m_lastError = L"Cannot open file for SHA256: " + filePath.wstring();
            return L"";
        }

        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        {
            m_lastError = L"CryptAcquireContextW failed.";
            return L"";
        }

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
        {
            m_lastError = L"CryptCreateHash failed.";
            CryptReleaseContext(hProv, 0);
            return L"";
        }

        std::vector<char> buffer(8192);
        while (file.good())
        {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize count = file.gcount();
            if (count <= 0)
            {
                break;
            }

            if (!CryptHashData(hHash,
                               reinterpret_cast<const BYTE*>(buffer.data()),
                               static_cast<DWORD>(count),
                               0))
            {
                m_lastError = L"CryptHashData failed.";
                CryptDestroyHash(hHash);
                CryptReleaseContext(hProv, 0);
                return L"";
            }
        }

        BYTE hash[32] = {};
        DWORD hashLen = static_cast<DWORD>(sizeof(hash));
        if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0))
        {
            m_lastError = L"CryptGetHashParam failed.";
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return L"";
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        std::wstringstream stream;
        stream << std::hex << std::setfill(L'0');
        for (DWORD i = 0; i < hashLen; ++i)
        {
            stream << std::setw(2) << static_cast<unsigned int>(hash[i]);
        }
        return ToLowerCopy(stream.str());
    }

    bool PackageInstaller::VerifyPackageIntegrity(const fs::path& zipPath, const std::wstring& expectedSha256)
    {
        if (expectedSha256.empty())
        {
            return true;
        }

        std::wstring normalizedExpected;
        normalizedExpected.reserve(expectedSha256.size());
        for (wchar_t c : expectedSha256)
        {
            if (!::iswspace(c))
            {
                normalizedExpected.push_back(c);
            }
        }
        normalizedExpected = ToLowerCopy(normalizedExpected);

        const std::wstring actual = CalculateFileSha256(zipPath);
        if (actual.empty())
        {
            return false;
        }

        if (actual != normalizedExpected)
        {
            m_lastError = L"Plugin package integrity check failed. Expected SHA256 " +
                normalizedExpected + L", got " + actual + L".";
            return false;
        }

        return true;
    }

    bool PackageInstaller::ValidatePluginManifest(const fs::path& pluginDir, const std::wstring& expectedPluginId)
    {
        const fs::path manifestPath = pluginDir / L"manifest.json";

        if (!fs::exists(manifestPath))
        {
            m_lastError = L"Plugin manifest.json not found in package.";
            return false;
        }

        std::ifstream file(manifestPath);
        if (!file.is_open())
        {
            m_lastError = L"Cannot open plugin manifest: " + manifestPath.wstring();
            return false;
        }

        json manifest;
        try
        {
            file >> manifest;
        }
        catch (const std::exception&)
        {
            m_lastError = L"Plugin manifest is not valid JSON.";
            return false;
        }

        if (!manifest.contains("id") || !manifest["id"].is_string())
        {
            m_lastError = L"Plugin manifest is missing required string field: id.";
            return false;
        }
        if (!manifest.contains("version") || !manifest["version"].is_string())
        {
            m_lastError = L"Plugin manifest is missing required string field: version.";
            return false;
        }
        if (!manifest.contains("displayName") || !manifest["displayName"].is_string())
        {
            m_lastError = L"Plugin manifest is missing required string field: displayName.";
            return false;
        }

        if (!expectedPluginId.empty())
        {
            const std::wstring manifestId = Utf8ToWide(manifest.value("id", std::string()));
            if (manifestId != expectedPluginId)
            {
                m_lastError = L"Plugin manifest id '" + manifestId + L"' does not match requested plugin id '" + expectedPluginId + L"'.";
                return false;
            }
        }

        return true;
    }

    fs::path PackageInstaller::ResolveExtractedPluginRoot(const fs::path& extractDir) const
    {
        std::error_code ec;
        if (fs::exists(extractDir / L"manifest.json"))
        {
            return extractDir;
        }

        std::vector<fs::path> childDirs;
        for (const auto& entry : fs::directory_iterator(extractDir, ec))
        {
            if (ec)
            {
                return {};
            }
            if (entry.is_directory())
            {
                childDirs.push_back(entry.path());
            }
        }

        if (childDirs.size() == 1)
        {
            const fs::path nested = childDirs.front();
            if (fs::exists(nested / L"manifest.json"))
            {
                return nested;
            }
        }

        return {};
    }

    fs::path PackageInstaller::BuildTempPath(const std::wstring& prefix, const std::wstring& pluginId) const
    {
        SYSTEMTIME st{};
        GetSystemTime(&st);
        const std::wstring stamp =
            std::to_wstring(st.wYear) +
            std::to_wstring(st.wMonth) +
            std::to_wstring(st.wDay) + L"_" +
            std::to_wstring(st.wHour) +
            std::to_wstring(st.wMinute) +
            std::to_wstring(st.wSecond) + L"_" +
            std::to_wstring(GetCurrentProcessId());
        return m_pluginBasePath / (L"_" + prefix + L"_" + pluginId + L"_" + stamp);
    }

    bool PackageInstaller::ReplacePluginWithRollback(const fs::path& targetPluginDir,
                                                     const fs::path& stagedPluginRoot,
                                                     bool keepBackup)
    {
        std::error_code ec;
        const std::wstring pluginId = targetPluginDir.filename().wstring();
        const fs::path backupPath = BuildTempPath(L"backup", pluginId);

        if (fs::exists(targetPluginDir))
        {
            fs::create_directories(backupPath.parent_path(), ec);
            if (!MoveDirectory(targetPluginDir, backupPath))
            {
                m_lastError = L"Failed to create rollback backup for plugin update.";
                return false;
            }
        }

        fs::create_directories(targetPluginDir, ec);
        bool installed = true;
        try
        {
            for (const auto& entry : fs::directory_iterator(stagedPluginRoot))
            {
                const fs::path dest = targetPluginDir / entry.path().filename();
                fs::rename(entry.path(), dest);
            }
        }
        catch (const std::exception&)
        {
            installed = false;
        }

        if (!installed)
        {
            fs::remove_all(targetPluginDir, ec);
            if (fs::exists(backupPath))
            {
                MoveDirectory(backupPath, targetPluginDir);
            }
            m_lastError = L"Plugin update failed while replacing files. Previous version restored.";
            return false;
        }

        if (!keepBackup && fs::exists(backupPath))
        {
            fs::remove_all(backupPath, ec);
        }

        return true;
    }

    bool PackageInstaller::MoveDirectory(const fs::path& from, const fs::path& to)
    {
        std::error_code ec;
        fs::rename(from, to, ec);
        if (!ec)
        {
            return true;
        }

        fs::create_directories(to.parent_path(), ec);
        ec.clear();
        fs::copy(from, to,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing,
                 ec);
        if (ec)
        {
            return false;
        }

        fs::remove_all(from, ec);
        return !ec;
    }

} // namespace PluginPackage

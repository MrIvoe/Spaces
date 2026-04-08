#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <memory>

namespace PluginPackage
{
    namespace fs = std::filesystem;

    // Result status for package operations
    enum class InstallStatus
    {
        Success,
        DownloadFailed,
        InvalidPackage,
        ExtractionFailed,
        ValidationFailed,
        InstallFailed,
        AlreadyInstalled,
        IncompatibleVersion
    };

    // Plugin package installer
    class PackageInstaller
    {
    public:
        PackageInstaller(const fs::path& pluginBasePath);
        ~PackageInstaller();

        // Download and install a plugin from URL
        // Returns true on success
        InstallStatus InstallFromUrl(
            const std::wstring& pluginId,
            const std::wstring& downloadUrl,
            const std::wstring& expectedSha256 = L"");

        // Install from already-downloaded zip file
        // Returns true on success
        InstallStatus InstallFromZip(
            const std::wstring& pluginId,
            const fs::path& zipPath);

        // Update an already-installed plugin from URL or zip.
        InstallStatus UpdateFromUrl(
            const std::wstring& pluginId,
            const std::wstring& downloadUrl,
            const std::wstring& expectedSha256 = L"",
            bool keepBackup = true);

        InstallStatus UpdateFromZip(
            const std::wstring& pluginId,
            const fs::path& zipPath,
            bool keepBackup = true);

        // Uninstall a plugin
        bool Uninstall(const std::wstring& pluginId);

        // Get last error message
        std::wstring GetLastError() const { return m_lastError; }

        // Verify plugin package integrity
        bool VerifyPackageIntegrity(const fs::path& zipPath, const std::wstring& expectedSha256);

        // Resolve path to plugin installation root.
        fs::path GetPluginBasePath() const { return m_pluginBasePath; }

    private:
        fs::path m_pluginBasePath;
        fs::path m_backupRootPath;
        std::wstring m_lastError;

        // Download file from URL
        bool DownloadFile(const std::wstring& url, const fs::path& saveTo);

        // Extract zip to destination
        bool ExtractZip(const fs::path& zipPath, const fs::path& targetDir);

        // Calculate SHA256 hash of a file
        std::wstring CalculateFileSha256(const fs::path& filePath);

        // Validate manifest.json in extracted plugin
        bool ValidatePluginManifest(const fs::path& pluginDir, const std::wstring& expectedPluginId = L"");

        // Extracted archive may contain either files at root or a single nested plugin folder.
        fs::path ResolveExtractedPluginRoot(const fs::path& extractDir) const;

        // Build deterministic temporary names under installer workspace.
        fs::path BuildTempPath(const std::wstring& prefix, const std::wstring& pluginId) const;

        bool ReplacePluginWithRollback(const fs::path& targetPluginDir,
                                       const fs::path& stagedPluginRoot,
                                       bool keepBackup);

        bool MoveDirectory(const fs::path& from, const fs::path& to);
    };

} // namespace PluginPackage

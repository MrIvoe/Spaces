#include <string>
#include <iostream>
#include <filesystem>
#include <ctime>
#include <cstdlib>
#include <fstream>

#include "core/SettingsStore.h"
#include "core/ThemeApplyPipeline.h"
#include "core/ThemePackageLoader.h"
#include "core/ThemePackageValidator.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "[FAIL] " << message << "\n";
        return 1;
    }

    std::filesystem::path FindRepoRoot()
    {
        std::filesystem::path current = std::filesystem::current_path();
        for (int i = 0; i < 8; ++i)
        {
            if (std::filesystem::exists(current / "CMakeLists.txt") &&
                std::filesystem::exists(current / "tests"))
            {
                return current;
            }

            if (!current.has_parent_path())
            {
                break;
            }
            current = current.parent_path();
        }

        return {};
    }

    std::filesystem::path GetUniqueTempPath()
    {
        static int counter = 0;
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        return tempDir / ("theme_failure_test_" + std::to_string(counter++) + "_" + std::to_string(std::time(nullptr)) + ".json");
    }

    class TempFileGuard
    {
    public:
        explicit TempFileGuard(const std::filesystem::path& path)
            : m_path(path)
        {
        }

        ~TempFileGuard()
        {
            try
            {
                if (std::filesystem::exists(m_path))
                    std::filesystem::remove(m_path);
                if (std::filesystem::exists(m_path.wstring() + L".tmp"))
                    std::filesystem::remove(m_path.wstring() + L".tmp");
            }
            catch (...)
            {
            }
        }

    private:
        std::filesystem::path m_path;
    };

    std::string EscapePowerShellSingleQuoted(const std::string& input)
    {
        std::string out;
        out.reserve(input.size() + 8);
        for (char c : input)
        {
            if (c == '\'')
            {
                out += "''";
            }
            else
            {
                out.push_back(c);
            }
        }
        return out;
    }

    bool CreateZipFromDirectory(const std::filesystem::path& sourceDir, const std::filesystem::path& zipPath)
    {
        std::error_code ec;
        std::filesystem::remove(zipPath, ec);

        const std::string source = EscapePowerShellSingleQuoted(sourceDir.string());
        const std::string dest = EscapePowerShellSingleQuoted(zipPath.string());
        const std::string command =
            "powershell -NoProfile -Command \"Compress-Archive -Path '" + source + "\\*' -DestinationPath '" + dest + "' -Force\"";

        return std::system(command.c_str()) == 0 && std::filesystem::exists(zipPath);
    }
}

int RunThemeFailureFocusedTests()
{
    const std::filesystem::path repoRoot = FindRepoRoot();
    if (repoRoot.empty())
    {
        return Fail("Theme failure tests: failed to locate repo root");
    }

    const std::filesystem::path fixtureRoot = repoRoot / "tests" / "fixtures" / "theme_packages";
    if (!std::filesystem::exists(fixtureRoot))
    {
        return Fail("Theme failure tests: fixture folder missing");
    }

    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "SimpleFencesThemeFailureZips";
    std::error_code ec;
    std::filesystem::create_directories(tempDir, ec);

    // Failure test 1: non-zip packages are rejected with explicit diagnostics.
    {
        const std::filesystem::path notZip = tempDir / "not_a_theme.txt";
        std::ofstream out(notZip, std::ios::binary | std::ios::trunc);
        out << "not a zip";
        out.close();

        ThemePackageValidator validator;
        const auto result = validator.ValidatePackage(notZip.wstring());
        if (result.isValid)
        {
            return Fail("Theme failure test 1: non-zip package should be rejected");
        }

        if (result.errorMessage.find(L".zip") == std::wstring::npos)
        {
            return Fail("Theme failure test 1: rejection reason should mention .zip requirement");
        }
    }

    // Failure test 2: invalid package rejection does not destabilize active theme settings.
    {
        const std::filesystem::path tempStorePath = GetUniqueTempPath();
        TempFileGuard guard(tempStorePath);

        SettingsStore store;
        store.Load(tempStorePath);

        ThemeApplyPipeline pipeline(&store);
        const auto baseline = pipeline.ApplyTheme(L"aurora-light");
        if (!baseline.success)
        {
            return Fail("Theme failure test 2: baseline theme apply should succeed");
        }

        const std::filesystem::path src = fixtureRoot / "invalid_missing_metadata";
        const std::filesystem::path zip = tempDir / "invalid_missing_metadata_for_failure.zip";
        if (!CreateZipFromDirectory(src, zip))
        {
            return Fail("Theme failure test 2: failed to create invalid fixture zip");
        }

        ThemePackageValidator validator;
        const auto result = validator.ValidatePackage(zip.wstring());
        if (result.isValid)
        {
            return Fail("Theme failure test 2: invalid package should be rejected");
        }

        if (store.Get(L"theme.win32.theme_id", L"") != L"aurora-light")
        {
            return Fail("Theme failure test 2: invalid package rejection should not overwrite active theme ID");
        }

        if (store.Get(L"theme.source", L"") != L"win32_theme_system")
        {
            return Fail("Theme failure test 2: invalid package rejection should keep canonical source");
        }
    }

    // Failure test 3: forbidden payload package is rejected and pipeline remains operational.
    {
        const std::filesystem::path tempStorePath = GetUniqueTempPath();
        TempFileGuard guard(tempStorePath);

        SettingsStore store;
        store.Load(tempStorePath);

        ThemeApplyPipeline pipeline(&store);
        if (!pipeline.ApplyTheme(L"graphite-office").success)
        {
            return Fail("Theme failure test 3: baseline apply should succeed");
        }

        const std::filesystem::path src = fixtureRoot / "invalid_forbidden_content";
        const std::filesystem::path zip = tempDir / "invalid_forbidden_for_failure.zip";
        if (!CreateZipFromDirectory(src, zip))
        {
            return Fail("Theme failure test 3: failed to create forbidden-content fixture zip");
        }

        ThemePackageValidator validator;
        const auto result = validator.ValidatePackage(zip.wstring());
        if (result.isValid)
        {
            return Fail("Theme failure test 3: forbidden-content package should be rejected");
        }

        if (!pipeline.ApplyTheme(L"nocturne-dark").success)
        {
            return Fail("Theme failure test 3: pipeline should remain operational after rejection");
        }

        if (store.Get(L"theme.win32.theme_id", L"") != L"nocturne-dark")
        {
            return Fail("Theme failure test 3: post-rejection theme apply should persist correctly");
        }
    }

    return 0;
}

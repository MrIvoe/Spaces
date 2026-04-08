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

    bool WriteUtf8File(const std::filesystem::path& path, const std::string& content)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            return false;
        }

        out << content;
        return out.good();
    }

    bool CreatePackageFolder(const std::filesystem::path& root,
                             const std::string& metadataJson,
                             const std::string& defaultTokensJson)
    {
        if (!WriteUtf8File(root / "theme-metadata.json", metadataJson))
        {
            return false;
        }

        return WriteUtf8File(root / "theme" / "tokens" / "default.json", defaultTokensJson);
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

    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "SimpleSpacesThemeFailureZips";
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

    // Failure test 4: metadata schema permutation (camelCase keys) is rejected.
    {
        const std::filesystem::path pkgRoot = tempDir / "invalid_schema_camel_case";
        std::filesystem::remove_all(pkgRoot, ec);
        std::filesystem::create_directories(pkgRoot, ec);

        const std::string metadata =
            "{\n"
            "  \"themeId\": \"camel-case-id\",\n"
            "  \"displayName\": \"Camel Case\",\n"
            "  \"version\": \"1.0.0\"\n"
            "}\n";
        const std::string tokens =
            "{\n"
            "  \"win32.base.window_color\": \"#101010\",\n"
            "  \"win32.base.text_color\": \"#EFEFEF\",\n"
            "  \"win32.base.accent_color\": \"#2288FF\"\n"
            "}\n";

        if (!CreatePackageFolder(pkgRoot, metadata, tokens))
        {
            return Fail("Theme failure test 4: failed to create package folder");
        }

        const std::filesystem::path zip = tempDir / "invalid_schema_camel_case.zip";
        if (!CreateZipFromDirectory(pkgRoot, zip))
        {
            return Fail("Theme failure test 4: failed to create schema-permutation zip");
        }

        ThemePackageValidator validator;
        const auto result = validator.ValidatePackage(zip.wstring());
        if (result.isValid)
        {
            return Fail("Theme failure test 4: schema-permutation metadata should be rejected");
        }
    }

    // Failure test 5: malformed metadata JSON is rejected.
    {
        const std::filesystem::path pkgRoot = tempDir / "invalid_metadata_json";
        std::filesystem::remove_all(pkgRoot, ec);
        std::filesystem::create_directories(pkgRoot, ec);

        const std::string metadata = "{\"theme_id\": \"broken-theme\", \"display_name\": \"Broken\",";
        const std::string tokens =
            "{\n"
            "  \"win32.base.window_color\": \"#101010\",\n"
            "  \"win32.base.text_color\": \"#EFEFEF\",\n"
            "  \"win32.base.accent_color\": \"#2288FF\"\n"
            "}\n";

        if (!CreatePackageFolder(pkgRoot, metadata, tokens))
        {
            return Fail("Theme failure test 5: failed to create malformed-metadata package");
        }

        const std::filesystem::path zip = tempDir / "invalid_metadata_json.zip";
        if (!CreateZipFromDirectory(pkgRoot, zip))
        {
            return Fail("Theme failure test 5: failed to zip malformed-metadata package");
        }

        ThemePackageValidator validator;
        const auto result = validator.ValidatePackage(zip.wstring());
        if (result.isValid)
        {
            return Fail("Theme failure test 5: malformed metadata JSON should be rejected");
        }
    }

    // Failure test 6: malformed token JSON is rejected.
    {
        const std::filesystem::path pkgRoot = tempDir / "invalid_tokens_json";
        std::filesystem::remove_all(pkgRoot, ec);
        std::filesystem::create_directories(pkgRoot, ec);

        const std::string metadata =
            "{\n"
            "  \"theme_id\": \"broken-token-json\",\n"
            "  \"display_name\": \"Broken Token JSON\",\n"
            "  \"version\": \"1.0.0\"\n"
            "}\n";
        const std::string tokens = "{\"tokens\": { \"win32.base.window_color\": \"#101010\" ";

        if (!CreatePackageFolder(pkgRoot, metadata, tokens))
        {
            return Fail("Theme failure test 6: failed to create malformed-token package");
        }

        const std::filesystem::path zip = tempDir / "invalid_tokens_json.zip";
        if (!CreateZipFromDirectory(pkgRoot, zip))
        {
            return Fail("Theme failure test 6: failed to zip malformed-token package");
        }

        ThemePackageValidator validator;
        const auto result = validator.ValidatePackage(zip.wstring());
        if (result.isValid)
        {
            return Fail("Theme failure test 6: malformed token JSON should be rejected");
        }
    }

    // Failure test 7: token schema permutation with invalid color values is rejected.
    {
        const std::filesystem::path pkgRoot = tempDir / "invalid_token_values";
        std::filesystem::remove_all(pkgRoot, ec);
        std::filesystem::create_directories(pkgRoot, ec);

        const std::string metadata =
            "{\n"
            "  \"theme_id\": \"invalid-token-values\",\n"
            "  \"display_name\": \"Invalid Token Values\",\n"
            "  \"version\": \"1.0.0\"\n"
            "}\n";
        const std::string tokens =
            "{\n"
            "  \"tokens\": {\n"
            "    \"win32.base.window_color\": \"not-a-hex\",\n"
            "    \"win32.base.text_color\": \"#12345\",\n"
            "    \"win32.base.accent_color\": 123\n"
            "  }\n"
            "}\n";

        if (!CreatePackageFolder(pkgRoot, metadata, tokens))
        {
            return Fail("Theme failure test 7: failed to create invalid-token-values package");
        }

        const std::filesystem::path zip = tempDir / "invalid_token_values.zip";
        if (!CreateZipFromDirectory(pkgRoot, zip))
        {
            return Fail("Theme failure test 7: failed to zip invalid-token-values package");
        }

        ThemePackageValidator validator;
        const auto result = validator.ValidatePackage(zip.wstring());
        if (result.isValid)
        {
            return Fail("Theme failure test 7: invalid token values should be rejected");
        }
    }

    // Failure test 8: mixed valid/invalid required tokens are rejected under partial corruption.
    {
        const std::filesystem::path pkgRoot = tempDir / "mixed_required_token_corruption";
        std::filesystem::remove_all(pkgRoot, ec);
        std::filesystem::create_directories(pkgRoot, ec);

        const std::string metadata =
            "{\n"
            "  \"theme_id\": \"mixed-token-corruption\",\n"
            "  \"display_name\": \"Mixed Token Corruption\",\n"
            "  \"version\": \"1.0.0\"\n"
            "}\n";
        // window/accent are valid, but required text token is malformed and should be filtered out.
        const std::string tokens =
            "{\n"
            "  \"tokens\": {\n"
            "    \"win32.base.window_color\": \"#202124\",\n"
            "    \"win32.base.text_color\": \"#GGGGGG\",\n"
            "    \"win32.base.accent_color\": \"#3B82F6\",\n"
            "    \"win32.base.border_color\": \"#E5E7EB\"\n"
            "  }\n"
            "}\n";

        if (!CreatePackageFolder(pkgRoot, metadata, tokens))
        {
            return Fail("Theme failure test 8: failed to create mixed-token-corruption package");
        }

        const std::filesystem::path zip = tempDir / "mixed_required_token_corruption.zip";
        if (!CreateZipFromDirectory(pkgRoot, zip))
        {
            return Fail("Theme failure test 8: failed to zip mixed-token-corruption package");
        }

        ThemePackageValidator validator;
        const auto result = validator.ValidatePackage(zip.wstring());
        if (result.isValid)
        {
            return Fail("Theme failure test 8: package with mixed required token corruption should be rejected");
        }

        if (result.errorMessage.find(L"required base tokens") == std::wstring::npos)
        {
            return Fail("Theme failure test 8: rejection should indicate required token coverage failure");
        }
    }

    // Failure test 9: required token in wrong namespace is rejected under partial corruption.
    {
        const std::filesystem::path pkgRoot = tempDir / "mixed_wrong_namespace_required_token";
        std::filesystem::remove_all(pkgRoot, ec);
        std::filesystem::create_directories(pkgRoot, ec);

        const std::string metadata =
            "{\n"
            "  \"theme_id\": \"mixed-wrong-namespace\",\n"
            "  \"display_name\": \"Mixed Wrong Namespace\",\n"
            "  \"version\": \"1.0.0\"\n"
            "}\n";
        // Required text token value exists, but under wrong namespace so win32 required coverage must fail.
        const std::string tokens =
            "{\n"
            "  \"tokens\": {\n"
            "    \"win32.base.window_color\": \"#202124\",\n"
            "    \"theme.base.text_color\": \"#F5F7FA\",\n"
            "    \"win32.base.accent_color\": \"#5090F6\",\n"
            "    \"win32.base.border_color\": \"#E5E7EB\"\n"
            "  }\n"
            "}\n";

        if (!CreatePackageFolder(pkgRoot, metadata, tokens))
        {
            return Fail("Theme failure test 9: failed to create wrong-namespace package");
        }

        const std::filesystem::path zip = tempDir / "mixed_wrong_namespace_required_token.zip";
        if (!CreateZipFromDirectory(pkgRoot, zip))
        {
            return Fail("Theme failure test 9: failed to zip wrong-namespace package");
        }

        ThemePackageValidator validator;
        const auto result = validator.ValidatePackage(zip.wstring());
        if (result.isValid)
        {
            return Fail("Theme failure test 9: wrong-namespace required token should be rejected");
        }

        if (result.errorMessage.find(L"required base tokens") == std::wstring::npos)
        {
            return Fail("Theme failure test 9: rejection should indicate required token coverage failure");
        }
    }

    // Failure test 10: required token keys with non-string value types are rejected.
    {
        const std::filesystem::path pkgRoot = tempDir / "required_token_wrong_value_type";
        std::filesystem::remove_all(pkgRoot, ec);
        std::filesystem::create_directories(pkgRoot, ec);

        const std::string metadata =
            "{\n"
            "  \"theme_id\": \"required-token-wrong-type\",\n"
            "  \"display_name\": \"Required Token Wrong Type\",\n"
            "  \"version\": \"1.0.0\"\n"
            "}\n";
        // Keys are correct, but required values are non-strings and must be ignored/rejected.
        const std::string tokens =
            "{\n"
            "  \"tokens\": {\n"
            "    \"win32.base.window_color\": [\"#202124\"],\n"
            "    \"win32.base.text_color\": { \"value\": \"#F5F7FA\" },\n"
            "    \"win32.base.accent_color\": \"#5090F6\",\n"
            "    \"win32.base.border_color\": \"#E5E7EB\"\n"
            "  }\n"
            "}\n";

        if (!CreatePackageFolder(pkgRoot, metadata, tokens))
        {
            return Fail("Theme failure test 10: failed to create wrong-type package");
        }

        const std::filesystem::path zip = tempDir / "required_token_wrong_value_type.zip";
        if (!CreateZipFromDirectory(pkgRoot, zip))
        {
            return Fail("Theme failure test 10: failed to zip wrong-type package");
        }

        ThemePackageValidator validator;
        const auto result = validator.ValidatePackage(zip.wstring());
        if (result.isValid)
        {
            return Fail("Theme failure test 10: non-string required token values should be rejected");
        }

        if (result.errorMessage.find(L"required base tokens") == std::wstring::npos)
        {
            return Fail("Theme failure test 10: rejection should indicate required token coverage failure");
        }
    }

    return 0;
}

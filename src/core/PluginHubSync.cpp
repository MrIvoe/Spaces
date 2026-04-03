#include "core/PluginHubSync.h"

#include "Win32Helpers.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
    bool RunProcess(const std::wstring& commandLine, const std::filesystem::path& cwd, DWORD& exitCode)
    {
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::vector<wchar_t> cmd(commandLine.begin(), commandLine.end());
        cmd.push_back(L'\0');

        std::wstring cwdText = cwd.wstring();

        const BOOL started = CreateProcessW(
            nullptr,
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

    std::wstring QuotePath(const std::filesystem::path& p)
    {
        return L"\"" + p.wstring() + L"\"";
    }

    std::wstring NormalizeRepoUrl(std::wstring repoUrl)
    {
        if (repoUrl.empty())
        {
            return repoUrl;
        }

        // Trim common whitespace.
        while (!repoUrl.empty() && iswspace(repoUrl.front()))
        {
            repoUrl.erase(repoUrl.begin());
        }
        while (!repoUrl.empty() && iswspace(repoUrl.back()))
        {
            repoUrl.pop_back();
        }

        // Accept shorthand like github.com/owner/repo.
        if (repoUrl.rfind(L"http://", 0) != 0 && repoUrl.rfind(L"https://", 0) != 0)
        {
            repoUrl = L"https://" + repoUrl;
        }

        // Strip trailing slash so .git appending behaves cleanly.
        while (!repoUrl.empty() && repoUrl.back() == L'/')
        {
            repoUrl.pop_back();
        }

        // Ensure git clone URL form.
        if (repoUrl.size() < 4 || repoUrl.substr(repoUrl.size() - 4) != L".git")
        {
            repoUrl += L".git";
        }

        return repoUrl;
    }
}

namespace PluginHubSync
{
    PluginHubSyncResult SyncFromRepository(const std::wstring& repoUrl, const std::wstring& branch)
    {
        PluginHubSyncResult result;

        if (repoUrl.empty())
        {
            result.message = L"Plugin hub repository URL is empty.";
            return result;
        }

        std::error_code ec;
        const std::wstring normalizedRepoUrl = NormalizeRepoUrl(repoUrl);
        std::vector<std::wstring> candidateBranches;
        if (branch.empty())
        {
            candidateBranches.push_back(L"main");
            candidateBranches.push_back(L"master");
        }
        else
        {
            candidateBranches.push_back(branch);
        }

        std::vector<std::wstring> candidateUrls;
        candidateUrls.push_back(normalizedRepoUrl);
        {
            // Friendly fallback for common typo: .../Simple-Fences-Plugin vs .../Simple-Fences-Plugins
            const std::wstring singular = L"Simple-Fences-Plugin.git";
            const std::wstring plural = L"Simple-Fences-Plugins.git";
            std::wstring retryUrl = normalizedRepoUrl;
            if (retryUrl.size() >= singular.size() && retryUrl.substr(retryUrl.size() - singular.size()) == singular)
            {
                retryUrl.replace(retryUrl.size() - singular.size(), singular.size(), plural);
                candidateUrls.push_back(retryUrl);
            }
        }

        std::wstring selectedBranch;
        std::wstring selectedRepoUrl;
        const std::filesystem::path appRoot = Win32Helpers::GetAppDataRoot();
        const std::filesystem::path hubCache = appRoot / L"PluginHubCache";
        const std::filesystem::path installRoot = appRoot / L"plugins";

        std::filesystem::create_directories(appRoot, ec);
        std::filesystem::create_directories(installRoot, ec);

        DWORD exitCode = 0;

        if (!std::filesystem::exists(hubCache / L".git"))
        {
            std::filesystem::create_directories(hubCache.parent_path(), ec);

            bool cloned = false;
            for (const auto& url : candidateUrls)
            {
                for (const auto& b : candidateBranches)
                {
                    const std::wstring cloneCmd =
                        L"git clone --depth 1 --branch \"" + b + L"\" \"" + url + L"\" " + QuotePath(hubCache);
                    if (RunProcess(cloneCmd, appRoot, exitCode) && exitCode == 0)
                    {
                        selectedBranch = b;
                        selectedRepoUrl = url;
                        cloned = true;
                        break;
                    }
                }
                if (cloned)
                {
                    break;
                }
            }

            if (!cloned)
            {
                result.message = L"Failed to clone plugin hub repo from URL: " + normalizedRepoUrl +
                                 L". Ensure git is installed and the repository/branch is reachable.";
                return result;
            }
        }
        else
        {
            selectedRepoUrl = normalizedRepoUrl;

            bool fetched = false;
            for (const auto& b : candidateBranches)
            {
                const std::wstring fetchCmd = L"git -C " + QuotePath(hubCache) + L" fetch origin \"" + b + L"\" --depth 1";
                if (RunProcess(fetchCmd, appRoot, exitCode) && exitCode == 0)
                {
                    selectedBranch = b;
                    fetched = true;
                    break;
                }
            }

            if (!fetched)
            {
                result.message = L"Failed to fetch updates from plugin hub repo.";
                return result;
            }

            const std::wstring resetCmd = L"git -C " + QuotePath(hubCache) + L" reset --hard \"origin/" + selectedBranch + L"\"";
            if (!RunProcess(resetCmd, appRoot, exitCode) || exitCode != 0)
            {
                result.message = L"Failed to reset local plugin hub cache to remote branch.";
                return result;
            }
        }

        const std::filesystem::path sourcePlugins = hubCache / L"plugins";
        if (!std::filesystem::exists(sourcePlugins) || !std::filesystem::is_directory(sourcePlugins))
        {
            result.message = L"Plugin hub cache does not contain a plugins directory.";
            return result;
        }

        int copied = 0;
        for (const auto& entry : std::filesystem::directory_iterator(sourcePlugins, ec))
        {
            ec.clear();
            if (!entry.is_directory())
            {
                continue;
            }

            const std::filesystem::path pluginName = entry.path().filename();
            const std::filesystem::path destination = installRoot / pluginName;

            std::filesystem::remove_all(destination, ec);
            ec.clear();
            std::filesystem::copy(
                entry.path(),
                destination,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                ec);

            if (!ec)
            {
                ++copied;
            }
        }

        result.success = true;
        result.installedPluginCount = copied;
        result.message = L"Plugin sync completed. Installed " + std::to_wstring(copied) +
                         L" plugin folder(s) from " + selectedRepoUrl + L" (branch " + selectedBranch +
                         L") to " + installRoot.wstring() + L".";

        Win32Helpers::LogInfo(L"PluginHubSync: " + result.message);
        return result;
    }
}

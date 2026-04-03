#include "App.h"
#include "AppVersion.h"
#include "FenceStorage.h"
#include "Persistence.h"
#include "FenceManager.h"
#include "TrayMenu.h"
#include "Win32Helpers.h"
#include "core/AppKernel.h"
#include "ui/SettingsWindow.h"
#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")

App::App() = default;

App::~App()
{
    Shutdown();
}

bool App::Initialize(HINSTANCE hInstance)
{
    Win32Helpers::LogInfo(L"App::Initialize starting (version=" + std::wstring(SimpleFencesVersion::kVersion) + L")");
    m_hInstance = hInstance;

    m_singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\SimpleFences.MainInstance");
    if (!m_singleInstanceMutex)
    {
        Win32Helpers::LogError(L"CreateMutexW failed for single-instance guard: " + std::to_wstring(GetLastError()));
    }
    else if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        Win32Helpers::ShowUserWarning(nullptr, L"SimpleFences", L"SimpleFences is already running.");
        Win32Helpers::LogError(L"Second instance prevented by single-instance guard.");
        CloseHandle(m_singleInstanceMutex);
        m_singleInstanceMutex = nullptr;
        return false;
    }

    // Initialize common controls (legacy but reliable)
    InitCommonControls();
    Win32Helpers::LogInfo(L"InitCommonControls done");

    Win32Helpers::LogInfo(L"Creating FenceStorage");
    // Create storage and persistence
    m_storage = std::make_unique<FenceStorage>();
    m_persistence = std::make_unique<Persistence>(m_storage->GetMetadataPath());

    Win32Helpers::LogInfo(L"Creating FenceManager");
    // Create manager
    m_manager = std::make_unique<FenceManager>(std::move(m_storage), std::move(m_persistence));

    Win32Helpers::LogInfo(L"Creating TrayMenu");
    m_kernel = std::make_unique<AppKernel>();
    if (!m_kernel->Initialize(this))
    {
        Win32Helpers::LogError(L"AppKernel::Initialize reported plugin load failures. Continuing with degraded plugin set.");
    }

    if (m_manager)
    {
        m_manager->SetFenceExtensionRegistry(m_kernel->GetFenceExtensionRegistry());
        m_manager->SetMenuContributionRegistry(m_kernel->GetMenuRegistry());
        m_manager->SetCommandExecutor([this](const std::wstring& commandId, const CommandContext& context) {
            return ExecuteCommand(commandId, context);
        });
        m_manager->SetThemePlatform(m_kernel->GetThemePlatform());
    }

    // Create tray icon
    m_tray = std::make_unique<TrayMenu>(this);
    if (!m_tray->Create(hInstance))
    {
        Win32Helpers::LogError(L"TrayMenu::Create failed");
        return false;
    }

    Win32Helpers::LogInfo(L"Loading persisted fences");
    // Load persisted fences
    if (!m_manager->LoadAll())
    {
        Win32Helpers::LogError(L"LoadAll failed");
        return false;
    }

    Win32Helpers::LogInfo(L"Initialize completed successfully");
    return true;
}

int App::Run()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

void App::Exit()
{
    Shutdown();
    PostQuitMessage(0);
}

std::wstring App::CreateFenceNearCursor()
{
    FenceCreateRequest request;
    return CreateFenceNearCursor(request);
}

std::wstring App::CreateFenceNearCursor(const FenceCreateRequest& request)
{
    if (!m_manager)
        return L"";

    POINT pt{};
    GetCursorPos(&pt);
    return m_manager->CreateFenceAt(pt.x, pt.y, request);
}

FenceManager* App::GetFenceManager() const
{
    return m_manager.get();
}

bool App::ExecuteCommand(const std::wstring& commandId) const
{
    if (!m_kernel)
    {
        return false;
    }

    return m_kernel->ExecuteCommand(commandId);
}

bool App::ExecuteCommand(const std::wstring& commandId, const CommandContext& context) const
{
    if (!m_kernel)
    {
        return false;
    }

    return m_kernel->ExecuteCommand(commandId, context);
}

std::vector<TrayMenuEntry> App::GetTrayMenuEntries() const
{
    if (!m_kernel)
    {
        return {};
    }

    return m_kernel->GetTrayMenuEntries();
}

std::vector<PluginStatusView> App::GetPluginStatuses() const
{
    if (!m_kernel)
    {
        return {};
    }

    return m_kernel->GetPluginStatuses();
}

std::vector<SettingsPageView> App::GetSettingsPages() const
{
    if (!m_kernel)
    {
        return {};
    }

    return m_kernel->GetSettingsPages();
}

const ThemePlatform* App::GetThemePlatform() const
{
    return m_kernel ? m_kernel->GetThemePlatform() : nullptr;
}

void App::OpenSettingsWindow()
{
    if (!m_settingsWindow)
    {
        m_settingsWindow = std::make_unique<SettingsWindow>();
    }

    m_settingsWindow->ShowScaffold(
        GetSettingsPages(),
        GetPluginStatuses(),
        m_kernel ? m_kernel->GetSettingsRegistry() : nullptr,
        m_kernel ? m_kernel->GetThemePlatform() : nullptr);
}

void App::Shutdown()
{
    if (m_shutdownStarted)
    {
        return;
    }

    m_shutdownStarted = true;
    Win32Helpers::LogInfo(L"App shutdown started");

    if (m_manager)
    {
        m_manager->SaveAll();
        m_manager->Shutdown();
    }

    if (m_tray)
    {
        m_tray->Destroy();
        m_tray.reset();
    }

    if (m_kernel)
    {
        m_kernel->Shutdown();
        m_kernel.reset();
    }

    m_settingsWindow.reset();

    m_manager.reset();
    m_persistence.reset();
    m_storage.reset();

    if (m_singleInstanceMutex)
    {
        ReleaseMutex(m_singleInstanceMutex);
        CloseHandle(m_singleInstanceMutex);
        m_singleInstanceMutex = nullptr;
    }

    Win32Helpers::LogInfo(L"App shutdown completed");
}

#pragma once
#include <memory>
#include <windows.h>

class FenceStorage;
class Persistence;
class FenceManager;
class TrayMenu;

class App
{
public:
    App();
    ~App();

    bool Initialize(HINSTANCE hInstance);
    int Run();
    void Exit();

    void CreateFenceNearCursor();
    FenceManager* GetFenceManager() const;

    HINSTANCE GetInstance() const { return m_hInstance; }

private:
    HINSTANCE m_hInstance = nullptr;
    std::unique_ptr<FenceStorage> m_storage;
    std::unique_ptr<Persistence> m_persistence;
    std::unique_ptr<FenceManager> m_manager;
    std::unique_ptr<TrayMenu> m_tray;
};

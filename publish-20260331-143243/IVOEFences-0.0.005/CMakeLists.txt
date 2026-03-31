cmake_minimum_required(VERSION 3.20)
project(IVOEFences LANGUAGES CXX)
option(IVOE_ENABLE_SHELL_INTEGRATION "Build optional shell integration DLL" OFF)
include(CTest)
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows subsystem — no console window
if(MSVC)
    add_compile_options(/W4 /WX- /permissive- /utf-8)
endif()

set(SOURCES
    src/main.cpp
    src/App.cpp
    src/AppPaths.cpp
    src/DesktopItemService.cpp
    src/DesktopWatcher.cpp
    src/FenceLayoutEngine.cpp
    src/FenceSelectionModel.cpp
    src/ShellDesktop.cpp
    src/FenceWindow.cpp
    src/FenceManager.cpp
    src/FenceRepository.cpp
    src/JsonPersistence.cpp
    src/TrayIcon.cpp
    src/ZOrderCoordinator.cpp
    src/ShellChangeWatcher.cpp
    src/PathMove.cpp
    src/UserIntegrityLauncher.cpp
)

add_executable(IVOEFences WIN32 ${SOURCES})

target_compile_definitions(IVOEFences PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

target_include_directories(IVOEFences PRIVATE src)

target_link_libraries(IVOEFences PRIVATE
    nlohmann_json::nlohmann_json
    comctl32
    shell32
    user32
    gdi32
    shlwapi
    advapi32
)

add_executable(UserActionHelper WIN32
    src/UserActionHelperMain.cpp
    src/PathMove.cpp
)

target_compile_definitions(UserActionHelper PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

target_include_directories(UserActionHelper PRIVATE src)

target_link_libraries(UserActionHelper PRIVATE
    shell32
    user32
    shlwapi
)

if(IVOE_ENABLE_SHELL_INTEGRATION)
    add_library(IVOEFencesShellIntegration SHARED
        src/shellintegration/ShellIntegration.cpp
    )

    target_include_directories(IVOEFencesShellIntegration PRIVATE src/shellintegration)
    target_link_libraries(IVOEFencesShellIntegration PRIVATE user32)
endif()

if(BUILD_TESTING)
    add_executable(IVOEFencesIntegrationTests
        tests/IntegrationTests.cpp
        src/AppPaths.cpp
        src/DesktopItemService.cpp
        src/FenceLayoutEngine.cpp
        src/FenceSelectionModel.cpp
        src/FenceRepository.cpp
        src/JsonPersistence.cpp
    )

    target_include_directories(IVOEFencesIntegrationTests PRIVATE src)
    target_compile_definitions(IVOEFencesIntegrationTests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(IVOEFencesIntegrationTests PRIVATE nlohmann_json::nlohmann_json shell32 user32)

    add_test(NAME IVOEFencesIntegrationTests COMMAND IVOEFencesIntegrationTests)
endif()

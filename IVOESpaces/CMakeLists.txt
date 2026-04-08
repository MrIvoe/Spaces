cmake_minimum_required(VERSION 3.20)
project(IVOESpaces LANGUAGES CXX)
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

set(IVOE_RUNTIME_OUTPUT_DIR "${CMAKE_SOURCE_DIR}")

function(ivoe_configure_exe_output target_name)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${IVOE_RUNTIME_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${IVOE_RUNTIME_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${IVOE_RUNTIME_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${IVOE_RUNTIME_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${IVOE_RUNTIME_OUTPUT_DIR}"
    )
endfunction()

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
    src/SpaceLayoutEngine.cpp
    src/SpaceSelectionModel.cpp
    src/ShellDesktop.cpp
    src/SpaceWindow.cpp
    src/SpaceManager.cpp
    src/SpaceRepository.cpp
    src/JsonPersistence.cpp
    src/TrayIcon.cpp
    src/ZOrderCoordinator.cpp
    src/ShellChangeWatcher.cpp
    src/PathMove.cpp
    src/UserIntegrityLauncher.cpp
)

add_executable(IVOESpaces WIN32 ${SOURCES})
ivoe_configure_exe_output(IVOESpaces)

target_compile_definitions(IVOESpaces PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

target_include_directories(IVOESpaces PRIVATE src)

target_link_libraries(IVOESpaces PRIVATE
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
ivoe_configure_exe_output(UserActionHelper)

target_compile_definitions(UserActionHelper PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

target_include_directories(UserActionHelper PRIVATE src)

target_link_libraries(UserActionHelper PRIVATE
    shell32
    user32
    shlwapi
)

if(IVOE_ENABLE_SHELL_INTEGRATION)
    add_library(IVOESpacesShellIntegration SHARED
        src/shellintegration/ShellIntegration.cpp
    )

    target_include_directories(IVOESpacesShellIntegration PRIVATE src/shellintegration)
    target_link_libraries(IVOESpacesShellIntegration PRIVATE user32)
endif()

if(BUILD_TESTING)
    add_executable(IVOESpacesIntegrationTests
        tests/IntegrationTests.cpp
        src/AppPaths.cpp
        src/DesktopItemService.cpp
        src/SpaceLayoutEngine.cpp
        src/SpaceSelectionModel.cpp
        src/SpaceRepository.cpp
        src/JsonPersistence.cpp
    )
    ivoe_configure_exe_output(IVOESpacesIntegrationTests)

    target_include_directories(IVOESpacesIntegrationTests PRIVATE src)
    target_compile_definitions(IVOESpacesIntegrationTests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(IVOESpacesIntegrationTests PRIVATE nlohmann_json::nlohmann_json shell32 user32)

    add_test(NAME IVOESpacesIntegrationTests COMMAND IVOESpacesIntegrationTests)
endif()

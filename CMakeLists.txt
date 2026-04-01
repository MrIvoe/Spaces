cmake_minimum_required(VERSION 3.16)
project(SimpleFences)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main executable
add_executable(SimpleFences WIN32
    src/main.cpp
    src/App.h
    src/App.cpp
    src/core/AppKernel.h
    src/core/AppKernel.cpp
    src/core/CommandDispatcher.h
    src/core/CommandDispatcher.cpp
    src/core/EventBus.h
    src/core/EventBus.cpp
    src/core/Diagnostics.h
    src/core/Diagnostics.cpp
    src/core/KernelViews.h
    src/core/ServiceRegistry.h
    src/core/ServiceRegistry.cpp
    src/core/SettingsStore.h
    src/core/SettingsStore.cpp
    src/extensions/PluginContracts.h
    src/extensions/SettingsSchema.h
    src/extensions/PluginHost.h
    src/extensions/PluginHost.cpp
    src/extensions/PluginRegistry.h
    src/extensions/PluginRegistry.cpp
    src/extensions/PluginSettingsRegistry.h
    src/extensions/PluginSettingsRegistry.cpp
    src/extensions/MenuContributionRegistry.h
    src/extensions/MenuContributionRegistry.cpp
    src/extensions/FenceExtensionRegistry.h
    src/extensions/FenceExtensionRegistry.cpp
    src/plugins/builtins/BuiltinPlugins.h
    src/plugins/builtins/BuiltinPlugins.cpp
    src/FenceStorage.h
    src/FenceStorage.cpp
    src/Persistence.h
    src/Persistence.cpp
    src/FenceManager.h
    src/FenceManager.cpp
    src/FenceWindow.h
    src/FenceWindow.cpp
    src/TrayMenu.h
    src/TrayMenu.cpp
    src/Win32Helpers.h
    src/Win32Helpers.cpp
    src/ui/SettingsWindow.h
    src/ui/SettingsWindow.cpp
    src/Models.h
)

target_include_directories(SimpleFences PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link Windows libraries
target_link_libraries(SimpleFences
    nlohmann_json::nlohmann_json
    user32
    shell32
    ole32
    comctl32
    shlwapi
    kernel32
)

# Set output directory
set_target_properties(SimpleFences PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Enable Unicode
if(MSVC)
    target_compile_options(SimpleFences PRIVATE /W4)
    target_compile_definitions(SimpleFences PRIVATE UNICODE _UNICODE)
else()
    target_compile_options(SimpleFences PRIVATE -Wall -Wextra)
    target_compile_definitions(SimpleFences PRIVATE UNICODE _UNICODE)
endif()

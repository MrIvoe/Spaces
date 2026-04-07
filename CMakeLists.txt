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
    src/core/PluginHubSync.h
    src/core/PluginHubSync.cpp
    src/core/ServiceRegistry.h
    src/core/ServiceRegistry.cpp
    src/core/SettingsStore.h
    src/core/SettingsStore.cpp
    src/core/ThemeMigrationService.h
    src/core/ThemeMigrationService.cpp
    src/core/PluginAppearanceConflictGuard.h
    src/core/PluginAppearanceConflictGuard.cpp
    src/core/ThemePackageValidator.h
    src/core/ThemePackageValidator.cpp
    src/core/ThemePackageLoader.h
    src/core/ThemePackageLoader.cpp
    src/core/ThemeTokenResolver.h
    src/core/ThemeTokenResolver.cpp
    src/core/ThemeApplyPipeline.h
    src/core/ThemeApplyPipeline.cpp
    src/core/ThemePlatform.h
    src/core/ThemePlatform.cpp
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
    comdlg32
    user32
    shell32
    ole32
    comctl32
    shlwapi
    dwmapi
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

enable_testing()

add_executable(HostCoreTests
    tests/HostCoreTests.cpp
    tests/plugins/PluginContractValidationTests.cpp
    tests/plugins/PluginUpdaterGatesTests.cpp
    tests/plugins/ThemeTokenPolicyTests.cpp
    tests/plugins/PluginSecurityPolicyTests.cpp
    tests/plugins/ThemeMigrationTests.cpp
    tests/plugins/ThemeRenderingTests.cpp
    tests/plugins/ThemeFailureTests.cpp
    tests/plugins/PluginLoaderConflictIntegrationTests.cpp
    tests/plugins/PluginHostRuntimeConflictTests.cpp
        tests/plugins/ThemeIntegrationTests.cpp
    tests/plugins/TelemetrySnapshotDiagnostics.cpp
    src/AppVersion.h
    src/core/CommandDispatcher.h
    src/core/CommandDispatcher.cpp
    src/core/Diagnostics.h
    src/core/Diagnostics.cpp
    src/core/SettingsStore.h
    src/core/SettingsStore.cpp
        src/core/ThemePackageLoader.h
        src/core/ThemePackageLoader.cpp
        src/core/ThemeTokenResolver.h
        src/core/ThemeTokenResolver.cpp
        src/core/ThemeApplyPipeline.h
        src/core/ThemeApplyPipeline.cpp
    src/core/ThemePlatform.h
    src/core/ThemePlatform.cpp
    src/Persistence.h
    src/Persistence.cpp
    src/Models.h
    src/extensions/PluginContracts.h
    src/extensions/MenuContributionRegistry.h
    src/extensions/MenuContributionRegistry.cpp
    src/extensions/PluginRegistry.h
    src/extensions/PluginRegistry.cpp
    src/extensions/PluginSettingsRegistry.h
    src/extensions/PluginSettingsRegistry.cpp
    src/extensions/FenceExtensionRegistry.h
    src/extensions/FenceExtensionRegistry.cpp
    src/extensions/PluginHost.h
    src/extensions/PluginHost.cpp
    src/plugins/builtins/BuiltinPlugins.h
    src/plugins/builtins/BuiltinPlugins.cpp
    src/plugins/host/PluginContractModels.h
    src/plugins/host/ThemeTokenPolicy.h
    src/plugins/host/PluginContractValidation.h
    src/plugins/host/PluginUpdaterGates.h
    src/core/ThemeMigrationService.h
    src/core/ThemeMigrationService.cpp
    src/core/PluginAppearanceConflictGuard.h
    src/core/PluginAppearanceConflictGuard.cpp
    src/core/ThemePackageValidator.h
    src/core/ThemePackageValidator.cpp
        src/core/ThemePackageLoader.h
        src/core/ThemePackageLoader.cpp
        src/core/ThemeTokenResolver.h
        src/core/ThemeTokenResolver.cpp
        src/core/ThemeApplyPipeline.h
        src/core/ThemeApplyPipeline.cpp
    src/Win32Helpers.h
    src/Win32Helpers.cpp
)

target_include_directories(HostCoreTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/plugins
)

if(MSVC)
    target_compile_options(HostCoreTests PRIVATE /W4)
    target_compile_definitions(HostCoreTests PRIVATE UNICODE _UNICODE)
else()
    target_compile_options(HostCoreTests PRIVATE -Wall -Wextra)
    target_compile_definitions(HostCoreTests PRIVATE UNICODE _UNICODE)
endif()

target_link_libraries(HostCoreTests
    nlohmann_json::nlohmann_json
    comdlg32
    user32
    shell32
    ole32
    comctl32
    shlwapi
    dwmapi
    kernel32
)

add_test(NAME HostCoreTests COMMAND HostCoreTests)

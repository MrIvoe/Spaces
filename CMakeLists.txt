cmake_minimum_required(VERSION 3.16)
project(SimpleFences)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main executable
add_executable(SimpleFences WIN32
    src/main.cpp
    src/App.h
    src/App.cpp
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
    src/Models.h
)

# Link Windows libraries
target_link_libraries(SimpleFences
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

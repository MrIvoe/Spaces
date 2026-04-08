#pragma once
#include <windows.h>
#include <string>

// Minimal utility helpers used across the project.

namespace Utils {

// Convert narrow string to wide.
inline std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), result.data(), len);
    return result;
}

// Convert wide string to narrow UTF-8.
inline std::string ToNarrow(const std::wstring& s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(),
                                   nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(),
                         result.data(), len, nullptr, nullptr);
    return result;
}

} // namespace Utils

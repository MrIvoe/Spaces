#pragma once

#include <string>

class Diagnostics
{
public:
    void Info(const std::wstring& message) const;
    void Error(const std::wstring& message) const;
};

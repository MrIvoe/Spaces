#pragma once
#include <string>
#include <vector>
#include "Models.h"

class Persistence
{
public:
    explicit Persistence(const std::wstring& metadataPath);
    ~Persistence() = default;

    bool LoadFences(std::vector<FenceModel>& fences);
    bool SaveFences(const std::vector<FenceModel>& fences);
    bool SaveFence(const FenceModel& fence);

private:
    bool EnsureDirectory();

    std::wstring m_metadataPath;
};

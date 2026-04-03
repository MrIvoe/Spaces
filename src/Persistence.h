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
    bool SaveTextAtomic(const std::string& text);
    bool QuarantineCorruptMetadata(const std::wstring& reason);

    std::wstring m_metadataPath;
};

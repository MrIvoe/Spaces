#pragma once
#include <string>
#include <vector>
#include "Models.h"

class Persistence
{
public:
    explicit Persistence(const std::wstring& metadataPath);
    ~Persistence() = default;

    bool LoadSpaces(std::vector<SpaceModel>& spaces);
    bool SaveSpaces(const std::vector<SpaceModel>& spaces);
    bool SaveSpace(const SpaceModel& space);

private:
    bool EnsureDirectory();
    bool SaveTextAtomic(const std::string& text);
    bool QuarantineCorruptMetadata(const std::wstring& reason);

    std::wstring m_metadataPath;
};

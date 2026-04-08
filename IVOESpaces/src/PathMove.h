#pragma once
#include <string>

namespace PathMove {

bool EnsureDirectory(const std::wstring& path);
std::wstring GetFileName(const std::wstring& path);
std::wstring BuildUniqueDestination(const std::wstring& destDir, const std::wstring& fileName);
bool MovePathToDirectory(const std::wstring& sourcePath, const std::wstring& destDir, std::wstring* outMovedTo = nullptr);
bool CopyPathToDirectory(const std::wstring& sourcePath, const std::wstring& destDir, std::wstring* outCopiedTo = nullptr);

} // namespace PathMove

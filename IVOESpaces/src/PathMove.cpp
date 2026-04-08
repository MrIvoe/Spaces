#include "PathMove.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>

namespace {

std::wstring JoinPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (left.back() == L'\\') {
        return left + right;
    }
    return left + L"\\" + right;
}

std::wstring BuildNumberedName(const std::wstring& name, int index) {
    const wchar_t* ext = PathFindExtensionW(name.c_str());
    if (!ext || *ext == L'\0') {
        return name + L" (" + std::to_wstring(index) + L")";
    }

    std::wstring base = name.substr(0, name.size() - wcslen(ext));
    return base + L" (" + std::to_wstring(index) + L")" + ext;
}

bool Transfer(const std::wstring& sourcePath, const std::wstring& destDir, bool copy, std::wstring* outPath) {
    if (sourcePath.empty() || destDir.empty()) {
        return false;
    }

    if (!PathFileExistsW(sourcePath.c_str())) {
        return false;
    }

    if (!PathMove::EnsureDirectory(destDir)) {
        return false;
    }

    std::wstring fileName = PathMove::GetFileName(sourcePath);
    std::wstring destination = PathMove::BuildUniqueDestination(destDir, fileName);

    wchar_t fromBuf[MAX_PATH + 2]{};
    wchar_t toBuf[MAX_PATH + 2]{};
    wcsncpy_s(fromBuf, sourcePath.c_str(), _TRUNCATE);
    wcsncpy_s(toBuf, destination.c_str(), _TRUNCATE);

    SHFILEOPSTRUCTW op{};
    op.wFunc = copy ? FO_COPY : FO_MOVE;
    op.pFrom = fromBuf;
    op.pTo = toBuf;
    op.fFlags = FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_SILENT;

    int result = SHFileOperationW(&op);
    if (result == 0 && !op.fAnyOperationsAborted) {
        if (outPath) {
            *outPath = destination;
        }
        return true;
    }

    return false;
}

} // namespace

namespace PathMove {

bool EnsureDirectory(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }

    if (PathFileExistsW(path.c_str())) {
        return true;
    }

    int created = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    return created == ERROR_SUCCESS || created == ERROR_ALREADY_EXISTS || PathFileExistsW(path.c_str());
}

std::wstring GetFileName(const std::wstring& path) {
    const wchar_t* file = PathFindFileNameW(path.c_str());
    if (!file || *file == L'\0') {
        return L"item";
    }
    return file;
}

std::wstring BuildUniqueDestination(const std::wstring& destDir, const std::wstring& fileName) {
    std::wstring candidate = JoinPath(destDir, fileName);
    if (!PathFileExistsW(candidate.c_str())) {
        return candidate;
    }

    for (int i = 2; i < 10000; ++i) {
        std::wstring numbered = JoinPath(destDir, BuildNumberedName(fileName, i));
        if (!PathFileExistsW(numbered.c_str())) {
            return numbered;
        }
    }

    return JoinPath(destDir, fileName + L"_copy");
}

bool MovePathToDirectory(const std::wstring& sourcePath, const std::wstring& destDir, std::wstring* outMovedTo) {
    return Transfer(sourcePath, destDir, false, outMovedTo);
}

bool CopyPathToDirectory(const std::wstring& sourcePath, const std::wstring& destDir, std::wstring* outCopiedTo) {
    return Transfer(sourcePath, destDir, true, outCopiedTo);
}

} // namespace PathMove

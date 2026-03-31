#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <fstream>

#include "FenceLayoutEngine.h"
#include "FenceRepository.h"
#include "FenceSelectionModel.h"

namespace {

std::filesystem::path CreateTestRoot() {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "ivoefences_phase1_tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    return root;
}

bool TestBackingFolderRoundTrip(const std::filesystem::path& rootPath) {
    FenceRepository repository(rootPath);
    FenceRepositoryState state;

    FenceData in;
    in.id = 7;
    in.title = L"TestFence";
    in.rect = RECT{10, 20, 310, 220};
    in.collapsed = true;
    in.backingFolder = (rootPath / "FenceStorage" / "Fence_7").wstring();

    state.fences = {in};
    state.dropPolicy = DropPolicy::Copy;
    state.showInfoNotifications = false;
    state.monitorSignature = FenceRepository::BuildMonitorSignature();

    if (!repository.Save(state)) {
        return false;
    }

    FenceRepositoryState out;
    if (!repository.Load(out)) {
        return false;
    }

    if (out.fences.size() != 1) {
        return false;
    }

    const FenceData& f = out.fences[0];
    return f.id == in.id
        && f.title == in.title
        && f.rect.left == in.rect.left
        && f.rect.top == in.rect.top
        && f.rect.right == in.rect.right
        && f.rect.bottom == in.rect.bottom
        && f.collapsed == in.collapsed
        && f.backingFolder == in.backingFolder
        && out.dropPolicy == DropPolicy::Copy
        && out.showInfoNotifications == false;
}

bool TestMonitorSignatureMismatchSkipsRestore(const std::filesystem::path& rootPath) {
    FenceRepository repository(rootPath);
    FenceRepositoryState state;

    FenceData in;
    in.id = 1;
    in.title = L"Fence";
    in.rect = RECT{40, 40, 400, 340};
    state.fences = {in};
    state.monitorSignature = L"definitely-mismatch";

    if (!repository.Save(state)) {
        return false;
    }

    FenceRepositoryState out;
    if (!repository.Load(out)) {
        return false;
    }

    return out.fences.empty();
}

bool TestConfigPathUsesJson(const std::filesystem::path& rootPath) {
    FenceRepository repository(rootPath);
    return repository.GetConfigPath().extension() == ".json";
}

bool TestAtomicSaveLeavesConfigPresent(const std::filesystem::path& rootPath) {
    FenceRepository repository(rootPath);
    FenceRepositoryState state;
    state.monitorSignature = FenceRepository::BuildMonitorSignature();

    if (!repository.Save(state)) {
        return false;
    }

    return std::filesystem::exists(repository.GetConfigPath())
        && !std::filesystem::exists(repository.GetConfigPath().parent_path() / "fences.json.tmp");
}

bool TestLegacyFolderMigrationCreatesMembership(const std::filesystem::path& rootPath) {
    FenceRepository repository(rootPath);

    const std::filesystem::path legacyFolder = rootPath / "FenceStorage" / "Fence_9";
    std::error_code error;
    std::filesystem::create_directories(legacyFolder, error);
    if (error) {
        return false;
    }

    std::ofstream marker(legacyFolder / "Example.txt");
    marker << "phase2";
    marker.close();

    FenceRepositoryState state;
    FenceData fence;
    fence.id = 9;
    fence.title = L"LegacyFence";
    fence.backingFolder = legacyFolder.wstring();
    state.fences = {fence};
    state.monitorSignature = FenceRepository::BuildMonitorSignature();

    if (!repository.Save(state)) {
        return false;
    }

    FenceRepositoryState out;
    if (!repository.Load(out) || out.fences.empty()) {
        return false;
    }

    const FenceData& loaded = out.fences.front();
    if (loaded.type != FenceType::Standard) {
        return false;
    }

    if (loaded.members.empty()) {
        return false;
    }

    return loaded.members.front().source == DesktopItemSource::LegacyFenceFolder;
}

bool TestFenceLayoutEngineDeterministic() {
    FenceLayoutEngine engine;
    RECT client{0, 0, 360, 300};
    const auto slots = engine.BuildListLayout(client, 3);
    if (slots.size() != 3) {
        return false;
    }

    POINT insideSecond{slots[1].bounds.left + 1, slots[1].bounds.top + 1};
    return engine.HitTest(slots, insideSecond) == 1;
}

bool TestFenceSelectionModelBasics() {
    FenceSelectionModel selection;
    selection.Reset(4);
    if (selection.GetItemCount() != 4) {
        return false;
    }

    selection.SelectSingle(2);
    if (!selection.IsSelected(2)) {
        return false;
    }
    if (selection.GetPrimarySelection() != 2) {
        return false;
    }

    selection.Clear();
    if (selection.GetPrimarySelection() != -1 || selection.IsSelected(2)) {
        return false;
    }

    selection.Reset(0);
    return selection.GetItemCount() == 0;
}

} // namespace

int main() {
    const std::filesystem::path rootPath = CreateTestRoot();

    const bool t1 = TestBackingFolderRoundTrip(rootPath);
    const bool t2 = TestMonitorSignatureMismatchSkipsRestore(rootPath);
    const bool t3 = TestConfigPathUsesJson(rootPath);
    const bool t4 = TestAtomicSaveLeavesConfigPresent(rootPath);
    const bool t5 = TestLegacyFolderMigrationCreatesMembership(rootPath);
    const bool t6 = TestFenceLayoutEngineDeterministic();
    const bool t7 = TestFenceSelectionModelBasics();

    if (!t1) {
        std::wcerr << L"TestBackingFolderRoundTrip failed\n";
    }
    if (!t2) {
        std::wcerr << L"TestMonitorSignatureMismatchSkipsRestore failed\n";
    }
    if (!t3) {
        std::wcerr << L"TestConfigPathUsesJson failed\n";
    }
    if (!t4) {
        std::wcerr << L"TestAtomicSaveLeavesConfigPresent failed\n";
    }
    if (!t5) {
        std::wcerr << L"TestLegacyFolderMigrationCreatesMembership failed\n";
    }
    if (!t6) {
        std::wcerr << L"TestFenceLayoutEngineDeterministic failed\n";
    }
    if (!t7) {
        std::wcerr << L"TestFenceSelectionModelBasics failed\n";
    }

    if (!(t1 && t2 && t3 && t4 && t5 && t6 && t7)) {
        std::wcerr << L"Integration tests failed\n";
        return 1;
    }

    std::wcout << L"Integration tests passed\n";
    return 0;
}

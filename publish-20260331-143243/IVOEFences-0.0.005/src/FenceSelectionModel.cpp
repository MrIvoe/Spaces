#include "FenceSelectionModel.h"

#include <algorithm>

void FenceSelectionModel::Reset(int itemCount) {
    const int clampedCount = std::max(0, itemCount);
    m_selected.assign(static_cast<size_t>(clampedCount), false);
    m_primarySelection = -1;
}

void FenceSelectionModel::Clear() {
    std::fill(m_selected.begin(), m_selected.end(), false);
    m_primarySelection = -1;
}

void FenceSelectionModel::SelectSingle(int index) {
    Clear();
    if (index < 0 || index >= static_cast<int>(m_selected.size())) {
        return;
    }

    m_selected[static_cast<size_t>(index)] = true;
    m_primarySelection = index;
}

bool FenceSelectionModel::IsSelected(int index) const {
    if (index < 0 || index >= static_cast<int>(m_selected.size())) {
        return false;
    }

    return m_selected[static_cast<size_t>(index)];
}

int FenceSelectionModel::GetPrimarySelection() const {
    return m_primarySelection;
}

int FenceSelectionModel::GetItemCount() const {
    return static_cast<int>(m_selected.size());
}
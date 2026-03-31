#pragma once

#include <vector>

class FenceSelectionModel {
public:
    void Reset(int itemCount);
    void Clear();
    void SelectSingle(int index);
    bool IsSelected(int index) const;
    int GetPrimarySelection() const;
    int GetItemCount() const;

private:
    std::vector<bool> m_selected;
    int m_primarySelection{-1};
};
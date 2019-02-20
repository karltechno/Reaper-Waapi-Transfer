#pragma once
#include <vector>
#include <unordered_map>
#include <Windows.h>

#include "types.h"

class WAAPITransfer;

class TransferSearch
{
public:
    TransferSearch(WAAPITransfer *transferObject, HWND parent, const int RegionListViewId, const int RegionTracksToRenderListViewId);
    ~TransferSearch() = default;

    TransferSearch(const TransferSearch&) = delete;
    TransferSearch operator=(const TransferSearch&) = delete;

    void RefreshState();

    HWND GetRegionListViewHWND() { return GetDlgItem(m_hwnd, m_regionListViewId); }
    HWND GetRegionTracksToRenderHWND() { return GetDlgItem(m_hwnd, m_regionTracksToRenderListViewId); }

    enum RegionListViewSubItemID
    {
        Name,
        NumRenderItems
    };

    enum RegionTracksToRenderListViewSubItemID
    {
        AudioFileName,
        RenderSource
    };

private:
    HWND m_hwnd;
    int m_regionListViewId;
    int m_regionTracksToRenderListViewId;

    void RefreshReaperState();
    void RefreshRenderItemIndexing();


    WAAPITransfer *const m_waapiTransfer;

    //Maps reaper track GUID to render items using it
    std::unordered_map<std::string, std::vector<RenderItemID>> m_trackGuidToRenderItems;

    //Maps reaper region id to render ids using it
    std::unordered_map<int32, std::vector<RenderItemID>> m_regionIdToRenderItems;
    std::unordered_map<int32, MappedListViewID> m_regionIdToMappedListView;

    
};
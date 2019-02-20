#include <algorithm>
#include <cctype>

#include "reaper_plugin_functions.h"

#include "TransferSearch.h"
#include "WAAPITransfer.h"


TransferSearch::TransferSearch(WAAPITransfer *transferObject, HWND parent, const int regionListViewId, const int RegionTracksToRenderListViewId)
    : m_waapiTransfer(transferObject)
    , m_hwnd(parent)
    , m_regionListViewId(regionListViewId)
    , m_regionTracksToRenderListViewId(RegionTracksToRenderListViewId)
{
    HWND regionView = GetRegionListViewHWND();
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = RegionListViewSubItemID::Name;
        column.pszText = "Name";
        ListView_InsertColumn(regionView, RegionListViewSubItemID::Name, &column);
    }

    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = RegionListViewSubItemID::NumRenderItems;
        column.pszText = "Render Items";
        ListView_InsertColumn(regionView, RegionListViewSubItemID::NumRenderItems, &column);
    }

    ListView_SetExtendedListViewStyle(regionView, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

    //region render items view
    HWND regionTracksToRenderView = GetRegionTracksToRenderHWND();
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = RegionTracksToRenderListViewSubItemID::AudioFileName;
        column.pszText = "Audio File";
        ListView_InsertColumn(regionView, RegionTracksToRenderListViewSubItemID::AudioFileName, &column);
    }

    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = RegionTracksToRenderListViewSubItemID::RenderSource;
        column.pszText = "Render Source";
        ListView_InsertColumn(regionView, RegionTracksToRenderListViewSubItemID::RenderSource, &column);
    }

    RefreshState();
}

void TransferSearch::RefreshState()
{
    RefreshReaperState();
    RefreshRenderItemIndexing();
}

void TransferSearch::RefreshReaperState()
{
    m_trackGuidToRenderItems.clear();
    int numTracks = CountTracks(0);

    m_trackGuidToRenderItems.reserve(numTracks);

    //enumerate reaper tracks
    for (int trackIdx = 0;
         trackIdx < numTracks; 
         ++trackIdx)
    {
        MediaTrack *track = GetTrack(nullptr, trackIdx);
        char name[256];
        GetSetMediaTrackInfo_String(track, "P_NAME", name, false);

        //name -> tolower for searching, can cause collisions with two names capitalized differently but thats an odd use case
        //and user can still select track and match GUID anyway.
        std::transform(std::begin(name), std::end(name), std::begin(name), [](char c) { return std::tolower(c); });

        char guidStr[64];
        guidToString(GetTrackGUID(track), guidStr);
        m_trackGuidToRenderItems.insert({ guidStr, {} });
    }

    m_regionIdToRenderItems.clear();
    int numRegions;
    CountProjectMarkers(0, nullptr, &numRegions);
    m_regionIdToRenderItems.reserve(numRegions);

    //enumerate regions
    bool isRegion;
    double start, end;
    const char *regionName;
    int regionIndex;

    for (int markerIdx = 0; /* */; ++markerIdx)
    {
        if (!EnumProjectMarkers(markerIdx, &isRegion, &start, &end, &regionName, &regionIndex))
        {
            break;
        }

        if (!isRegion)
        {
            continue;
        }

        char regionNameStrBuffer[512];
        strcpy(regionNameStrBuffer, regionName);

        LVITEM listViewItem{};
        listViewItem.mask = LVIF_TEXT | LVIF_PARAM;
        listViewItem.iItem = static_cast<int>(m_regionIdToMappedListView.size());
        listViewItem.iSubItem = 0;
        listViewItem.pszText = regionNameStrBuffer;
        listViewItem.lParam = regionIndex;

        const int listViewId = ListView_InsertItem(GetRegionListViewHWND(), &listViewItem);
        const uint32 mappedListViewID = ListView_MapIndexToID(GetRegionListViewHWND(), listViewId);
        m_regionIdToMappedListView.insert({ regionIndex, mappedListViewID });

        m_regionIdToRenderItems.insert({ regionIndex, {} });
        
        //reaper gives us an immutable string
        std::string regionStr(regionName);
        std::transform(regionStr.begin(), regionStr.end(), regionStr.begin(), [](char c) { return std::tolower(c); });
    }
}

void TransferSearch::RefreshRenderItemIndexing()
{
    //iterate render items
    for (auto it = m_waapiTransfer->s_renderQueueItems.begin(),
         itEnd = m_waapiTransfer->s_renderQueueItems.end();
         it != itEnd;
         ++it)
    {
        const RenderItem &renderItem = it->second.first;
        const RenderItemID &renderItemId = it->first;

        //check render track, add if guid's match
        auto trackGuidFoundIt = m_trackGuidToRenderItems.find(renderItem.trackStemGuid);
        if (trackGuidFoundIt != m_trackGuidToRenderItems.end())
        {
            trackGuidFoundIt->second.push_back(renderItemId);
        }

        //check region id, add if match
        auto regionIdxFoundIt = m_regionIdToRenderItems.find(renderItem.reaperRegionId);
        if (regionIdxFoundIt != m_regionIdToRenderItems.end())
        {
            regionIdxFoundIt->second.push_back(renderItemId);
        }
    }
}

#include <algorithm>
#include <cctype>

#include "reaper_plugin_functions.h"

#include "TransferSearch.h"
#include "WAAPITransfer.h"

void TransferSearch::RefreshState()
{
    RefreshReaperState();
    RefreshRenderItemIndexing();
}

void TransferSearch::RefreshReaperState()
{
    m_trackGuidToRenderItems.clear();
    m_trackNameToGuid.clear();
    int numTracks = CountTracks(0);

    m_trackGuidToRenderItems.reserve(numTracks);
    m_trackNameToGuid.reserve(numTracks);

    //enumerate reaper tracks
    int trackIdx = 0;
    for (auto track = GetTrack(nullptr, trackIdx);
         track; 
         ++trackIdx)
    {
        char name[256];
        GetSetMediaTrackInfo_String(track, "P_NAME", name, false);

        //name -> tolower for searching, can cause collisions with two names capitalized differently but thats an odd use case
        //and user can still select track and match GUID anyway.
        std::transform(std::begin(name), std::end(name), std::begin(name), [](char c) {return std::tolower(c); });

        char guidStr[64];
        guidToString(GetTrackGUID(track), guidStr);
        m_trackGuidToRenderItems.insert({ guidStr, {} });

        auto sameNameFoundIt = m_trackNameToGuid.find(guidStr);
        if (sameNameFoundIt != m_trackNameToGuid.end())
        {
            sameNameFoundIt->second.push_back(guidStr);
        }
        else
        {
            m_trackNameToGuid.insert({ name, { guidStr } });
        }
    }

    m_regionNameToId.clear();
    m_regionIdToRenderItems.clear();
    int numRegions;
    CountProjectMarkers(0, nullptr, &numRegions);
    m_regionNameToId.reserve(numRegions);
    m_regionIdToRenderItems.reserve(numRegions);

    //enumerate regions
    bool isRegion;
    double start, end;
    const char *regionName;
    int regionIndex;

    for (int markerIdx = 0,
         resultIndex = EnumProjectMarkers(markerIdx, &isRegion, &start, &end, &regionName, &regionIndex);
         resultIndex != -1;
         ++markerIdx)
    {
        if (!isRegion)
        {
            continue;
        }

        m_regionIdToRenderItems.insert({ regionIndex, {} });
        
        //reaper gives us an immutable string
        std::string regionStr(regionName);
        std::transform(regionStr.begin(), regionStr.end(), regionStr.begin(), [](char c) { return std::tolower(c); });
        auto sameNameFoundIt = m_regionNameToId.find(regionName);

        if (sameNameFoundIt != m_regionNameToId.end())
        {
            sameNameFoundIt->second.push_back(regionIndex);
        }
        else
        {
            m_regionNameToId.insert({ std::move(regionStr), { regionIndex } });
        }
    }
}

void TransferSearch::RefreshRenderItemIndexing()
{
    //iterate render items
    for (auto it = m_waapiTransfer->renderQueueItems.begin(),
         itEnd = m_waapiTransfer->renderQueueItems.end();
         it != itEnd;
         ++it)
    {
        const auto &renderItem = it->second.first;
        const auto &renderItemId = it->first;

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

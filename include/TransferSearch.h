#pragma once
#include <vector>
#include <unordered_map>

#include "types.h"

class WAAPITransfer;

class TransferSearch
{
    explicit TransferSearch(WAAPITransfer *transferObject)
        : m_waapiTransfer(transferObject)
    {}

    void RefreshState();

private:
    void RefreshReaperState();
    void RefreshRenderItemIndexing();

    WAAPITransfer *const m_waapiTransfer;

    //Maps reaper track GUID to render items using it
    std::unordered_map<std::string, std::vector<RenderItemID>> m_trackGuidToRenderItems;
    //Maps reaper track name to GUID, NOTE: multiple tracks can have the same name, so we store a vector of GUIDs
    std::unordered_map<std::string, std::vector<std::string>> m_trackNameToGuid;

    //Maps reaper region id to render ids using it
    std::unordered_map<int32, std::vector<RenderItemID>> m_regionIdToRenderItems;
    //Maps reaper region name to id (like track, possible to have multiple with same name..)
    std::unordered_map<std::string, std::vector<int32>> m_regionNameToId;
};
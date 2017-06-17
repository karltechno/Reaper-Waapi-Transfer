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

    void UpdateReaperState();
    void UpdateRenderItemIndexing();

private:
    WAAPITransfer *const m_waapiTransfer;

    //Maps reaper track GUID to render items using it
    std::unordered_map<std::string, std::vector<RenderItemID>> m_trackToRenderItems;

    //Maps reaper track name to GUID, NOTE: multiple tracks can have the same name, so we store a vector of GUIDs
    std::unordered_map<std::string, std::vector<std::string>> m_trackNameToGuid;

    std::unordered_map<uint32, std::vector<RenderItemID>> m_render;
};
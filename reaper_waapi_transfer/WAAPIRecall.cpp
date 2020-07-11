#include <sstream>

#include "reaper_plugin_functions.h"

#include "Reaper_WAAPI_Transfer.h"
#include "WAAPIRecall.h"
#include "WAAPIHelpers.h"
#include "config.h"
#include "types.h"

WAAPIRecall::WAAPIRecall(HWND hwnd, int recallListId, int statusTextId)
    : hwnd(hwnd)
    , m_recallListId(recallListId)
    , m_statusTextId(statusTextId)
{
}

void WAAPIRecall::UpdateWwiseObjects()
{
    using namespace AK::WwiseAuthoringAPI;
    AkJson results;
    if (!GetAllSelectedWwiseObjects(results, m_client, true))
    {
        return;
    }

    AkJson::Array resultsArray;
    GetWaapiResultsArray(resultsArray, results);

    std::vector<RecallItem> validItems;

    //----------------------------------------------------------------
    //lambda for creating and inserting valid recallitems
    auto RecallItemInserter = [&validItems](const AkJson &item) -> void
    {
        const std::string notes = item["notes"].GetVariant().GetString();
        auto noteProjPos = notes.find(PROJ_NOTE_PREFIX);
        if (noteProjPos == notes.npos)
        {
            return;
        }

        const std::size_t projStringStart = notes.find_first_of('\"', noteProjPos);
        const std::size_t projStringEnd = notes.find_first_of('\"', projStringStart + 1);
        if (projStringEnd == notes.npos || projStringStart == notes.npos)
        {
            return;
        }

        RecallItem validItem;
        validItem.projectPath = notes.substr(projStringStart + 1, projStringEnd - projStringStart - 1);

        if (!fs::is_regular_file(validItem.projectPath))
        {
            return;
        }

        validItem.wwiseGuid = item["id"].GetVariant().GetString();
        validItem.wwiseName = item["name"].GetVariant().GetString();

        validItems.push_back(validItem);
    };
    //----------------------------------------------------------------

    //Build vector of recall items
    for (const auto &itemAkMap : resultsArray)
    {
        const std::string itemType = itemAkMap["type"].GetVariant().GetString();
        //if its a sound type we need the children (its possible there are multiple sources)
        //todo recursion depth search
        if (itemType == "Sound" || "MusicTrack")
        {
            //no children, continue
            if (!itemAkMap["childrenCount"].GetVariant().GetInt32())
            {
                continue;
            }

            AkJson results;
            if (!GetChildren(itemAkMap["path"].GetVariant().GetString(), results, m_client, true))
            {
                SetStatusText(GetResultsErrorMessage(results));
                continue;
            }
            else
            {
                AkJson::Array resultsArray;
                GetWaapiResultsArray(resultsArray, results);
                for (const auto &result : resultsArray)
                {
                    if (result["type"].GetVariant().GetString() == "AudioFileSource")
                    {
                        RecallItemInserter(result);
                    }
                }
            }
        }
        //this is the type notes waapi transfer exports are stored in
        else if (itemType == "AudioFileSource")
        {
            RecallItemInserter(itemAkMap);
        }
    }

    //delete items not in new selection
    for (auto it = m_mappedIdToRecallObject.begin();
         it != m_mappedIdToRecallObject.end();
         /* */)
    {
        auto found = std::find_if(validItems.begin(),
                                  validItems.end(),
                                  [it](const RecallItem &item)
        { return item.wwiseGuid == it->second.wwiseGuid; });

        if (found == validItems.end())
        {
            it = RemoveRecallItem(it);
        }
        else
        {
            ++it;
        }
    }

    for (const auto &newItem : validItems)
    {
        if (m_cachedGuids.find(newItem.wwiseGuid) == m_cachedGuids.end())
        {
            AddRecallItem(newItem);
        }
    }
}

void WAAPIRecall::OpenSelectedProject()
{
    HWND dlg = GetRecallListHWND();
    int index = ListView_GetNextItem(dlg, -1, LVNI_SELECTED);

    if (index == -1)
    {
        //nothing selected
        return;
    }

    uint32 mappedId = ListView_MapIndexToID(dlg, index);
    auto iter = m_mappedIdToRecallObject.find(mappedId);
    assert(iter != m_mappedIdToRecallObject.end());
    const auto path = iter->second.projectPath;

    //get open project and compare
    char currentProject[MAX_PATH];
    EnumProjects(-1, currentProject, MAX_PATH);

    //set window focus if project is open, TODO: check tabs 
    if (!strcmp(currentProject, path.c_str()))
    {
        SetActiveWindow(g_parentWindow);
        return;
    }

    if (fs::exists(path))
    {
        Main_openProject(path.c_str());
    }
    else
    {
        std::string message("Project not found: \n" + path);
        MessageBox(hwnd, message.c_str(), "File not found", MB_ICONERROR);
    }
}

WAAPIRecall::RecallItem &WAAPIRecall::GetRecallItem(uint32 mappedId)
{
    auto foundItem = m_mappedIdToRecallObject.find(mappedId);
    assert(foundItem != m_mappedIdToRecallObject.end());
    return foundItem->second;
}

void WAAPIRecall::SetStatusText(const std::string &status)
{
    if (m_currentStatusText != status)
    {
        m_currentStatusText = status;
        SetWindowText(GetStatusTextHWND(), status.c_str());
    }
}


void WAAPIRecall::AddRecallItem(const RecallItem &recallItem)
{
    LVITEM listItem{};
    listItem.mask = LVIF_TEXT;
    listItem.iItem = static_cast<int>(m_mappedIdToRecallObject.size());
    listItem.iSubItem = 0;
    char strbuff[256];
    strcpy(strbuff, recallItem.wwiseName.c_str());
    listItem.pszText = strbuff;

    HWND dlg = GetRecallListHWND();
    //get unique id for the list item
    int insertedItem = ListView_InsertItem(dlg, &listItem);
    uint32 mappedId = ListView_MapIndexToID(dlg, insertedItem);

    m_mappedIdToRecallObject.insert({ mappedId, recallItem });
    m_cachedGuids.insert(recallItem.wwiseGuid);
}

WAAPIRecall::RecallMap::iterator WAAPIRecall::RemoveRecallItem(const std::string &wwiseGuid)
{
    auto found = std::find_if(m_mappedIdToRecallObject.begin(),
                              m_mappedIdToRecallObject.end(),
                              [&wwiseGuid](const std::pair<uint32, RecallItem> &item)
    { return item.second.wwiseGuid == wwiseGuid; });

    return RemoveRecallItem(found);
}

WAAPIRecall::RecallMap::iterator WAAPIRecall::RemoveRecallItem(uint32 mappedId)
{
    return RemoveRecallItem(m_mappedIdToRecallObject.find(mappedId));
}

WAAPIRecall::RecallMap::iterator WAAPIRecall::RemoveRecallItem(RecallMap::iterator iter)
{
    assert(iter != m_mappedIdToRecallObject.end());
    HWND dlg = GetRecallListHWND();
    ListView_DeleteItem(dlg, ListView_MapIDToIndex(dlg, iter->first));
    m_cachedGuids.erase(iter->second.wwiseGuid);
    return m_mappedIdToRecallObject.erase(iter);
}

bool WAAPIRecall::Connect()
{
    using namespace AK::WwiseAuthoringAPI;
    AkJson wwiseInfo;
    bool success = false;

    if (success = m_client.Connect("127.0.0.1", g_Waapi_Port))
    {
        //Get Wwise info
        if (success = m_client.Call(ak::wwise::core::getInfo,
                                    AkJson(AkJson::Type::Map),
                                    AkJson(AkJson::Type::Map),
                                    wwiseInfo))
        {
            //create a status text string and set it
            std::stringstream status;
            status << "Connected on port " + std::to_string(g_Waapi_Port) + ": ";
            status << wwiseInfo["displayName"].GetVariant().GetString();
            status << " - " + wwiseInfo["version"]["displayName"].GetVariant().GetString();
            SetStatusText(status.str());
        }
    }

    if (!success)
    {
        SetStatusText("Failed to connect to Waapi on port: " + std::to_string(g_Waapi_Port));
    }

    return success;
}

void WAAPIRecall::SetupWindow()
{
    //Setup columns
    HWND recallHWND = GetRecallListHWND();
    int columnId = 0;
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = columnId;
        column.pszText = "Wwise Object";
        ListView_InsertColumn(recallHWND, columnId++, &column);
    }
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = columnId;
        column.pszText = "Project Name";
        ListView_InsertColumn(recallHWND, columnId++, &column);
    }
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 250;
        column.iSubItem = columnId;
        column.pszText = "Project Path";
        ListView_InsertColumn(recallHWND, columnId++, &column);
    }
    //extended styles
    ListView_SetExtendedListViewStyle(recallHWND, LVS_EX_FULLROWSELECT);
}
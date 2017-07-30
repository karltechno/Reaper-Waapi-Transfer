#define NOMINMAX

#include <string>
#include <sstream>
#include <thread>
#include <algorithm>
#include <chrono>

#include "resource.h"

#include "reaper_plugin_functions.h"

#include "Reaper_WAAPI_Transfer.h"
#include "WAAPITransfer.h"
#include "TransferWindowHandler.h"
#include "WAAPIHelpers.h"
#include "types.h"

#include "WwiseSettingsReader.h"

WAAPITransfer::WAAPITransfer(HWND window, int treeId, int statusTextid, int transferWindowId)
    : hwnd(window)
    , m_wwiseViewId(treeId)
    , m_statusTextId(statusTextid)
    , m_transferWindowId(transferWindowId)
    , m_progressWindow(0)
    , m_client(WAAPI_CLIENT_TIMEOUT_MS)
{
    Connect();
}

void WAAPITransfer::RecreateWwiseView()
{
    auto wwiseView = GetWwiseObjectListHWND();
    m_wwiseListViewMap.clear();
    ListView_DeleteAllItems(wwiseView);
    //setup columns
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = WwiseViewSubItemID::Name;
        column.pszText = "Name";
        ListView_InsertColumn(wwiseView, WwiseViewSubItemID::Name, &column);
    }
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 275;
        column.iSubItem = WwiseViewSubItemID::Path;
        column.pszText = "Path";
        ListView_InsertColumn(wwiseView, WwiseViewSubItemID::Path, &column);
    }
    ListView_SetExtendedListViewStyle(wwiseView, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
    ListView_SetImageList(wwiseView, WwiseImageList::GetImageList(), LVSIL_SMALL);

    for (const auto &item : activeWwiseObjects)
    { 
        AddWwiseObjectToView(item.first, item.second);
    }
}

void WAAPITransfer::RecreateTransferListView()
{
    ListView_DeleteAllItems(GetRenderViewHWND());

    //Setup columns
    HWND renderView = GetRenderViewHWND();
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = RenderViewSubitemID::AudioFileName;
        column.pszText = "File";
        ListView_InsertColumn(renderView, RenderViewSubitemID::AudioFileName, &column);
    }
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = RenderViewSubitemID::WwiseParent;
        column.pszText = "Wwise Parent";
        ListView_InsertColumn(renderView, RenderViewSubitemID::WwiseParent, &column);
    }
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 75;
        column.iSubItem = RenderViewSubitemID::WwiseImportObjectType;
        column.pszText = "Import Type";
        ListView_InsertColumn(renderView, RenderViewSubitemID::WwiseImportObjectType, &column);
    }
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = RenderViewSubitemID::WwiseLanguage;
        column.pszText = "Language (For Dialog)";
        ListView_InsertColumn(renderView, RenderViewSubitemID::WwiseLanguage, &column);
    }
    {
        LVCOLUMN column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = 125;
        column.iSubItem = RenderViewSubitemID::WaapiImportOperation;
        column.pszText = "Import Operation";
        ListView_InsertColumn(renderView, RenderViewSubitemID::WaapiImportOperation, &column);
    }

    ListView_SetExtendedListViewStyle(renderView, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

    for (auto &renderItemPair : renderQueueItems)
    {
        //update mapped id cached in the render item
        renderItemPair.second.second = AddRenderItemToView(renderItemPair.first, renderItemPair.second.first);
    }
}



void WAAPITransfer::SetSelectedRenderParents(MappedListViewID wwiseId)
{
    assert(wwiseId != -1);

    //loop selected listview items
    auto wwiseIter = m_wwiseListViewMap.find(wwiseId);
    assert(wwiseIter != m_wwiseListViewMap.end());

    const auto &wwiseGuid = wwiseIter->second;

    bool isMusicSegment = GetWwiseObjectByGUID(wwiseGuid).isMusicSegment;
    
    ForEachSelectedRenderItem([this, &wwiseGuid, isMusicSegment]
    (uint32 mappedIndex, uint32 index)
    {
        SetRenderItemWwiseParent(mappedIndex, wwiseGuid, isMusicSegment);
    });
}


void WAAPITransfer::SetSelectedImportObjectType(ImportObjectType typeToSet)
{
    const std::string text = GetTextForImportObject(typeToSet);
    char strBuff[256];
    strcpy(strBuff, text.c_str());
    bool musicTypeMismatch = false;

    ForEachSelectedRenderItem([this, &strBuff, &typeToSet, &musicTypeMismatch](MappedListViewID mappedIndex, uint32 listItem)
    {
        auto &renderItem = GetRenderItemFromListviewId(mappedIndex);
        bool isParentMusicSegment = false;
        if (renderItem.wwiseGuid != "")
        {
            isParentMusicSegment = GetWwiseObjectByGUID(renderItem.wwiseGuid).isMusicSegment;
        }

        //check if we are allowed to set this
        if (isParentMusicSegment)
        {
            if (typeToSet != ImportObjectType::Music)
            {
                musicTypeMismatch = true;
                return;
            }
        }
        else
        {
            //parent isnt music, make sure we arent setting to music
            if (typeToSet == ImportObjectType::Music)
            {
                musicTypeMismatch = true;
                return;
            }
        }

        renderItem.importObjectType = typeToSet;
        ListView_SetItemText(GetRenderViewHWND(), listItem, RenderViewSubitemID::WwiseImportObjectType, strBuff);
    });

    if (musicTypeMismatch)
    {
        SetStatusText("Music segments can only contain music objects.");
    }
}

void WAAPITransfer::SetSelectedDialogLanguage(int wwiseLanguageIndex)
{
    char strbuff[MAX_PATH];
    strcpy(strbuff, WwiseLanguages[wwiseLanguageIndex]);

    ForEachSelectedRenderItem([wwiseLanguageIndex, &strbuff, this](MappedListViewID mapped, uint32 index) 
    {
        GetRenderItemFromListviewId(mapped).wwiseLanguageIndex = wwiseLanguageIndex;
        ListView_SetItemText(GetRenderViewHWND(), index, RenderViewSubitemID::WwiseLanguage, strbuff);
    });
}

void WAAPITransfer::UpdateRenderQueue()
{
    auto projectFilesToAdd = GetRenderQueueProjectFiles();

    std::vector<RenderProjectMap::iterator> toDelete;

    for (auto iter = renderQueueCachedProjects.begin(), iterEnd = renderQueueCachedProjects.end();
         iter != iterEnd;
         ++iter)
    {
        if (!fs::exists(iter->first))
        {
            RemoveRenderItemsByProject(iter);
            toDelete.push_back(iter);
        }
    }

    for (auto iter : toDelete)
    {
        renderQueueCachedProjects.erase(iter);
    }

    //add render items that need to be added
    std::for_each(projectFilesToAdd.begin(), 
                  projectFilesToAdd.end(),
                  [this](const fs::path &path) 
                  {
                   auto iter = renderQueueCachedProjects.find(path.generic_string());

                   if (iter == renderQueueCachedProjects.end())
                   {
                       AddRenderItemsByProject(path);
                   }
                   });
}

RenderItem &WAAPITransfer::GetRenderItemFromRenderItemId(RenderItemID renderItemId)
{
    //maybe we should change to nested maps, oh well
    auto iter = renderQueueItems.find(renderItemId);
    assert(iter != renderQueueItems.end());
    return iter->second.first;
}

RenderItem &WAAPITransfer::GetRenderItemFromListviewId(MappedListViewID mappedListId)
{
    auto renderId = m_renderListViewMap.find(mappedListId);
    assert(renderId != m_renderListViewMap.end());
    return GetRenderItemFromRenderItemId(renderId->second);
}

WwiseObject &WAAPITransfer::GetWwiseObjectByGUID(const std::string &guid)
{
    auto foundIt = activeWwiseObjects.find(guid);
    assert(foundIt != activeWwiseObjects.end());
    return foundIt->second;
}


bool WAAPITransfer::Connect()
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

void WAAPITransfer::RunRenderQueueAndImport()
{
    //empty, no work to do
    if (renderQueueItems.empty())
    {
        SetStatusText("Nothing to render.");
        return;
    }

    //reconnect to make sure wwise is still connected before we render
    if (!Connect())
    {
        SetStatusText("Render cancelled, couldn't connect to Wwise.");
        return;
    }
    
    //find out how many items haven't been targeted to wwise
    uint32_t numEmptyRenders = std::count_if(renderQueueItems.begin(), renderQueueItems.end(),
                                             [](const auto &item)
                                             { return item.second.first.wwiseGuid.empty(); });
    
    if (numEmptyRenders)
    {
        //nothing selected, just return
        if (numEmptyRenders == renderQueueItems.size())
        {
            SetStatusText("No Wwise parents selected.");
            return;
        }
        else
        {
            std::string mboxText = std::to_string(numEmptyRenders) 
                + " items do not have Wwise parents and will be rendered but not imported"
                  "\nWould you like to render anyway?";

            int mboxReturn = MessageBox(g_parentWindow, mboxText.c_str(), 
                                        "WAAPI Transfer", MB_YESNO);

            if (mboxReturn != IDYES)
            {
                return;
            }
        }
    }

    char reaprojectPath[MAX_PATH];
    EnumProjects(-1, reaprojectPath, MAX_PATH);

    if (!strcmp(reaprojectPath, ""))
    {
        int mboxReturn = MessageBox(g_parentWindow, "You have not saved this Reaper session, project recall will not work"
                                    "\nWould you like to render anyway?",
                                    "WAAPI Transfer", MB_YESNO);

        if (mboxReturn != IDYES)
        {
            return;
        }
    }

    //success, start import
    m_closeTransferThreadByUser = false;
    std::thread(&WAAPITransfer::WaapiImportLoop, this).detach();
    OpenProgressWindow(hwnd, this);
}

void WAAPITransfer::SetStatusText(const std::string &status) const
{
    SetWindowText(GetStatusTextHWND(), status.c_str());
}

void WAAPITransfer::AddSelectedWwiseObjects()
{
    using namespace AK::WwiseAuthoringAPI;

    AkJson results;

    if (!GetAllSelectedWwiseObjects(results, m_client))
    {
        SetStatusText("WAAPI Error: " + GetResultsErrorMessage(results));
        return;
    }
    
    AkJson::Array resultsArray;
    GetWaapiResultsArray(resultsArray, results);

    uint32 numItemsAdded = 0;
    for (const auto &result : resultsArray)
    {
        const std::string wwiseObjectGuid = result["id"].GetVariant().GetString();

        //check if we already have this item
        auto cachedGuid = activeWwiseObjects.find(wwiseObjectGuid);
        if (cachedGuid != activeWwiseObjects.end())
        {
            continue;
        }

        //check if type is valid for importing audio
        const std::string wwiseObjectType = result["type"].GetVariant().GetString();
        if (!IsParentType(wwiseObjectType))
        {
            continue;
        }

        WwiseObject wwiseNode;
        wwiseNode.type = wwiseObjectType;
        wwiseNode.path = result["path"].GetVariant().GetString();
        wwiseNode.name = result["name"].GetVariant().GetString();
        wwiseNode.isMusicSegment = wwiseObjectType == "MusicSegment";

        CreateWwiseObject(wwiseObjectGuid, wwiseNode);
        ++numItemsAdded;
    }
            
    if (numItemsAdded)
    {
        SetStatusText(std::to_string(numItemsAdded) + " Wwise objects added.");
    }
    else
    {
        SetStatusText("No new parent Wwise objects selected.");
    }

}

void WAAPITransfer::RemoveWwiseObject(MappedListViewID toRemove)
{
    auto treeIter = m_wwiseListViewMap.find(toRemove);

    if (treeIter != m_wwiseListViewMap.end())
    {
        const auto &renderIdSet = GetWwiseObjectByGUID(treeIter->second).renderChildren;
        for (const RenderItemID& id : renderIdSet)
        {
            RemoveRenderItemWwiseParent(id);
        }

        activeWwiseObjects.erase(treeIter->second);
        m_wwiseListViewMap.erase(treeIter);
        HWND wwiseView = GetWwiseObjectListHWND();
        ListView_DeleteItem(wwiseView, ListView_MapIDToIndex(wwiseView, toRemove));
    }
}

void WAAPITransfer::RemoveAllWwiseObjects()
{
    for (auto it = renderQueueItems.begin(), 
         endIt = renderQueueItems.end();
         it != endIt;
         ++it)
    {
        RemoveRenderItemWwiseParent(it);
    }
    ListView_DeleteAllItems(GetWwiseObjectListHWND());
    activeWwiseObjects.clear();
    m_wwiseListViewMap.clear();
}

void WAAPITransfer::SetupAndRecreateWindow()
{
    RecreateTransferListView();
    RecreateWwiseView();
}


MappedListViewID WAAPITransfer::CreateWwiseObject(const std::string &guid, const WwiseObject &wwiseInfo)
{
    activeWwiseObjects.insert({ guid, wwiseInfo });
    
    return AddWwiseObjectToView(guid, wwiseInfo);
}

MappedListViewID WAAPITransfer::AddWwiseObjectToView(const std::string &guid, const WwiseObject &wwiseObject)
{
    auto wwiseListview = GetWwiseObjectListHWND();

    char strbuff[WWISE_NAME_MAX_LEN];
    strcpy(strbuff, wwiseObject.name.c_str());

    LVITEM item{};
    item.mask = LVIF_TEXT | LVIF_IMAGE;
    item.iItem = static_cast<int>(m_wwiseListViewMap.size());
    item.iSubItem = 0;
    item.pszText = strbuff;

    //icon
    item.iImage = WwiseImageList::GetIconForWwiseType(wwiseObject.type);

    //get unique id for the list item
    int insertedItem = ListView_InsertItem(wwiseListview, &item);
    uint32 mappedId = ListView_MapIndexToID(wwiseListview, insertedItem);
    m_wwiseListViewMap.insert({ mappedId, guid });

    {
        char strBuff[MAX_PATH];
        strcpy(strBuff, wwiseObject.path.c_str());
        ListView_SetItemText(wwiseListview, insertedItem, WwiseViewSubItemID::Path, strBuff);
    }
    return mappedId;
}


void WAAPITransfer::RemoveRenderItemsByProject(const fs::path &projectPath)
{
    auto iter = renderQueueCachedProjects.find(projectPath.generic_string());
    assert(iter != renderQueueCachedProjects.end());
    RemoveRenderItemsByProject(iter);
}

void WAAPITransfer::RemoveRenderItemsByProject(RenderProjectMap::iterator it) 
{
    for (uint32 renderId : it->second)
    {
        RemoveRenderItemFromList(renderId);
    }
}

void WAAPITransfer::AddRenderItemsByProject(const fs::path &path)
{
    //for testing 
    auto newRenderItems = ParseRenderQueue(path);
    //auto newRenderItems = ParseRenderQueueFile(path);
    //add to render project cache
    std::vector<uint32> renderIds;
    renderIds.reserve(newRenderItems.size());
    for (auto& renderItem : newRenderItems)
    {
        //get previous import type
        renderItem.importOperation = lastImportOperation;
        //add lvitem
        renderIds.push_back(CreateRenderItem(renderItem));
    }
    renderQueueCachedProjects.insert({ path.generic_string(), renderIds });
}

RenderItemID WAAPITransfer::CreateRenderItem(const RenderItem &renderItem)
{
    const RenderItemID renderId = RenderItemIdCounter++;
    renderQueueItems.insert({ renderId, { renderItem, AddRenderItemToView(renderId, renderItem) } });
    return renderId;
}

MappedListViewID WAAPITransfer::AddRenderItemToView(RenderItemID renderId, const RenderItem &renderItem)
{
    char text[WWISE_NAME_MAX_LEN];
    strcpy(text, renderItem.outputFileName.c_str());
    LVITEM item{};
    item.mask = LVIF_TEXT;
    item.iItem = static_cast<int>(m_renderListViewMap.size());
    item.iSubItem = 0;
    item.pszText = text;

    HWND dlg = GetRenderViewHWND();
    //get unique id for the list item
    int insertedItem = ListView_InsertItem(dlg, &item);
    MappedListViewID mappedId = ListView_MapIndexToID(dlg, insertedItem);

    {
        const std::string importText = GetTextForImportObject(renderItem.importObjectType);
        char strBuff[MAX_PATH];
        strcpy(strBuff, importText.c_str());
        ListView_SetItemText(dlg, insertedItem, RenderViewSubitemID::WwiseImportObjectType, strBuff);
    }
    {
        char strBuff[MAX_PATH];
        strcpy(strBuff, WwiseLanguages[renderItem.wwiseLanguageIndex]);
        ListView_SetItemText(dlg, insertedItem, RenderViewSubitemID::WwiseLanguage, strBuff);
    }
    {
        char strBuff[MAX_PATH];
        strcpy(strBuff, renderItem.wwiseParentName.c_str());
        ListView_SetItemText(dlg, insertedItem, RenderViewSubitemID::WwiseParent, strBuff);
    }
    {
        const std::string importString = GetImportOperationString(renderItem.importOperation);
        char strBuff[MAX_PATH];
        strcpy(strBuff, importString.c_str());
        ListView_SetItemText(dlg, insertedItem, RenderViewSubitemID::WaapiImportOperation, strBuff);
    }

    m_renderListViewMap.insert({ mappedId, renderId });
    return mappedId;
}


RenderItemMap::iterator WAAPITransfer::RemoveRenderItemFromList(RenderItemMap::iterator it)
{
    //remove from wwise parent in wwise object view
    const std::string& wwiseParentGuid = it->second.first.wwiseGuid;
    if (wwiseParentGuid != "")
    {
        GetWwiseObjectByGUID(wwiseParentGuid).renderChildren.erase(it->first);
    }

    const HWND listView = GetRenderViewHWND();
    int indexToRemove = ListView_MapIDToIndex(listView, it->second.second);
    //remove from listview 
    ListView_DeleteItem(listView, indexToRemove);
    //remove from listview id map
    m_renderListViewMap.erase(it->second.second);
    //remove from render queue items
    return renderQueueItems.erase(it);
}

RenderItemMap::iterator WAAPITransfer::RemoveRenderItemFromList(uint32 renderItemId)
{
    auto iter = renderQueueItems.find(renderItemId);
    assert(iter != renderQueueItems.end());
    return RemoveRenderItemFromList(iter);
}


void WAAPITransfer::WaapiImportLoop()
{
    using namespace AK::WwiseAuthoringAPI;

    for (const auto &renderQueueItemPath : renderQueueCachedProjects)
    {
        //make a copy of the project file if we fail
        fs::path copy = renderQueueItemPath.first + RENDER_QUEUE_BACKUP_APPEND;
        fs::copy_file(renderQueueItemPath.first, copy);
    }

    //Get reaper project path for tagging in wwise notes
    char reaprojectPath[MAX_PATH];
    EnumProjects(-1, reaprojectPath, MAX_PATH);
    const std::string projSourceNote(PROJ_NOTE_PREFIX + '"' + reaprojectPath + '"');

    //start reaper render
    PostMessage(hwnd, WM_TRANSFER_THREAD_MSG, 
                TRANSFER_THREAD_WPARAM::LAUNCH_RENDER_QUEUE_REQUEST, 0);

    std::size_t totalRenderItems = renderQueueCachedProjects.size();
    uint32 numItemsProcessed = 0;
    bool renderQueueActive = true;
    bool allImportsSucceeded = true;
    std::vector<std::string> backupsToRestore;

    RenderProjectMap renderQueueProjectsCopy = renderQueueCachedProjects;

    while (renderQueueActive && !m_closeTransferThreadByUser)
    {
        for (auto iter = renderQueueProjectsCopy.begin();
             iter != renderQueueProjectsCopy.end();
             /* */)
        {
            //cancel pressed
            if (m_closeTransferThreadByUser)
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            //reaper deletes the file when it's done rendering
            if (!fs::exists(iter->first))
            {
                int progressBarStep = static_cast<int>
                    ((++numItemsProcessed / (float)(totalRenderItems * 100.0f)));

                if (WaapiImportByProject(iter->first, projSourceNote))
                {
                    //inform main thread with new progress bar %                   
                    PostMessage(hwnd, WM_TRANSFER_THREAD_MSG, TRANSFER_THREAD_WPARAM::IMPORT_SUCCESS, progressBarStep);

                    //success, delete backup
                    fs::remove(iter->first + RENDER_QUEUE_BACKUP_APPEND);

                    //RemoveRenderItemsByProject(iter);
                    iter = renderQueueProjectsCopy.erase(iter);
                }
                else
                {
                    //inform main thread with new progress bar %
                    PostMessage(hwnd, WM_TRANSFER_THREAD_MSG, TRANSFER_THREAD_WPARAM::IMPORT_FAIL, progressBarStep);

                    allImportsSucceeded = false;

                    //restore this backup, something failed
                    backupsToRestore.push_back(iter->first);
                    iter = renderQueueProjectsCopy.erase(iter);
                }

                //if empty we are done
                if (renderQueueProjectsCopy.empty())
                {
                    renderQueueActive = false;
                    break;        
                }
            }
            else
            {
                ++iter;
            }
        }
    }

    for (const std::string &file : backupsToRestore)
    {
        fs::rename(file + RENDER_QUEUE_BACKUP_APPEND, file);
    }

    WPARAM importWparam;
    if (m_closeTransferThreadByUser)
    {
        importWparam = THREAD_EXIT_BY_USER;
    }
    else
    {
        importWparam = allImportsSucceeded ? THREAD_EXIT_SUCCESS : THREAD_EXIT_FAIL;
    }
    PostMessage(hwnd, WM_TRANSFER_THREAD_MSG, importWparam, 0);
}

bool WAAPITransfer::WaapiImportByProject(const std::string &projectPath, const std::string &RecallProjectPath)
{
    auto iter = renderQueueCachedProjects.find(projectPath);
    assert(iter != renderQueueCachedProjects.end());
    return WaapiImportByProject(iter, RecallProjectPath);
}


bool WAAPITransfer::WaapiImportByProject(RenderProjectMap::iterator projectIter, const std::string &RecallProjectPath)
{
    using namespace AK::WwiseAuthoringAPI;

    //wwise seems to crash importing a lot of items at once, so let's split it up into 10's for now
    const std::vector<uint32> &renderIdVec = projectIter->second;

    //we need seperate arrays for each import operation
    AkJson::Array itemsCreateNew;
    AkJson::Array itemsUseExisting;
    AkJson::Array itemsReplaceExisting;

    itemsCreateNew.reserve(WAAPI_IMPORT_BATCH_SIZE);
    itemsUseExisting.reserve(WAAPI_IMPORT_BATCH_SIZE);
    itemsReplaceExisting.reserve(WAAPI_IMPORT_BATCH_SIZE);

    bool allSucceeded = true;
    uint32 numBatches = static_cast<uint32>(std::ceilf(renderIdVec.size() / static_cast<float>(WAAPI_IMPORT_BATCH_SIZE)));

    for (uint32 batch = 0; batch < numBatches; ++batch)
    {
        itemsCreateNew.clear();
        itemsUseExisting.clear();
        itemsReplaceExisting.clear();

        uint32 numItems = std::min(static_cast<uint32>(renderIdVec.size()) - batch * WAAPI_IMPORT_BATCH_SIZE, WAAPI_IMPORT_BATCH_SIZE);

        for (uint32 i = 0; i < numItems; ++i)
        {
            const RenderItem &renderItem = GetRenderItemFromRenderItemId(renderIdVec[i + (batch * WAAPI_IMPORT_BATCH_SIZE)]);
            
            //check if object has wwise GUID attached
            if (renderItem.wwiseGuid == "")
            {
                continue;
            }

            AkJson importItem = AkJson(AkJson::Map{
                { "audioFile", AkVariant(renderItem.audioFilePath.generic_string()) },
                { "importLocation", AkVariant(renderItem.wwiseGuid) },
                { "objectType", AkVariant("Sound") },
                { "audioSourceNotes", AkVariant(RecallProjectPath) }
            });
            auto &importMap = importItem.GetMap();

            switch (renderItem.importObjectType)
            {

            case ImportObjectType::SFX:
            {
                importMap.insert({ "objectPath", AkVariant("<Sound SFX>" + renderItem.outputFileName) });
            } break;

            case ImportObjectType::Voice:
            {
                importMap.insert({ "importLanguage", AkVariant(WwiseLanguages[renderItem.wwiseLanguageIndex]) });
                importMap.insert({ "objectPath", AkVariant("<Sound Voice>" + renderItem.outputFileName) });
            } break;

            case ImportObjectType::Music:
            {
                importMap.insert({ "objectPath", AkVariant("<Music Track>" + renderItem.outputFileName) });
            } break;

            }

            switch (renderItem.importOperation)
            {
            case WAAPIImportOperation::createNew:
            {
                itemsCreateNew.push_back(importItem);
            } break;

            case WAAPIImportOperation::replaceExisting:
            {
                itemsReplaceExisting.push_back(importItem);
            } break;

            case WAAPIImportOperation::useExisting:
            {
                itemsUseExisting.push_back(importItem);
            } break;

            }
        }
        if (!itemsCreateNew.empty())
        {
            if (!WaapiImportItems(itemsCreateNew, m_client, WAAPIImportOperation::createNew)) allSucceeded = false;
        }
        if (!itemsReplaceExisting.empty())
        {
            if (!WaapiImportItems(itemsReplaceExisting, m_client, WAAPIImportOperation::replaceExisting)) allSucceeded = false;
        }
        if (!itemsUseExisting.empty())
        {
            if (!WaapiImportItems(itemsUseExisting, m_client, WAAPIImportOperation::useExisting)) allSucceeded = false;
        }
    }

    return allSucceeded;
}

void WAAPITransfer::SetSelectedImportOperation(WAAPIImportOperation operation)
{
    ForEachSelectedRenderItem([this, operation](uint32 mappedIndex, uint32 listItem)
    {
        GetRenderItemFromListviewId(mappedIndex).importOperation = operation;
        const std::string importStr = GetImportOperationString(operation);
        char operationStrbuff[64];
        strcpy(operationStrbuff, importStr.c_str());
        ListView_SetItemText(GetRenderViewHWND(), listItem, WAAPITransfer::RenderViewSubitemID::WaapiImportOperation, operationStrbuff);
    });
}

void WAAPITransfer::ForEachSelectedRenderItem(std::function<void(MappedListViewID, uint32)> func) const
{
    HWND listView = GetRenderViewHWND();
    int listItem = SendMessage(listView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
    while (listItem != -1)
    {
        //mapped index then index
        func(ListView_MapIndexToID(listView, listItem), listItem);
        listItem = SendMessage(listView, LVM_GETNEXTITEM, listItem, LVNI_SELECTED);
    }
}

void WAAPITransfer::SetRenderItemWwiseParent(MappedListViewID mappedIndex, const std::string &wwiseParentGuid, bool isMusicSegment)
{
    const auto renderIndexPair = m_renderListViewMap.find(mappedIndex);
    assert(renderIndexPair != m_renderListViewMap.end());

    RenderItem &item = GetRenderItemFromRenderItemId(renderIndexPair->second);

    //remove previous render id from wwise object internal map
    if (item.wwiseGuid != "")
    {
        GetWwiseObjectByGUID(item.wwiseGuid).renderChildren.erase(renderIndexPair->second);
    }

    auto &newWwiseParent = GetWwiseObjectByGUID(wwiseParentGuid);
    newWwiseParent.renderChildren.insert(renderIndexPair->second);

    const std::string wwiseParentName = newWwiseParent.name;
    item.wwiseGuid = wwiseParentGuid;
    item.wwiseParentName = wwiseParentName;

    char strbuff[WWISE_NAME_MAX_LEN];
    strcpy(strbuff, wwiseParentName.c_str());
    int listItem = ListView_MapIDToIndex(GetRenderViewHWND(), mappedIndex);
    ListView_SetItemText(GetRenderViewHWND(), listItem, RenderViewSubitemID::WwiseParent, strbuff);

    //we need to change the import object type if the parent is set to a music track
    if (isMusicSegment)
    {
        item.importObjectType = ImportObjectType::Music;
        const auto musicStr = GetTextForImportObject(ImportObjectType::Music);
        char musicStrBuffer[256];
        strcpy(musicStrBuffer, musicStr.c_str());
        ListView_SetItemText(GetRenderViewHWND(), listItem, RenderViewSubitemID::WwiseImportObjectType, musicStrBuffer);
    }
    else
    {
        //if it's already music type then we need to change it back, 
        //potentially could check somehow whether it should be dialog? - not high priority anyways..
        if (item.importObjectType == ImportObjectType::Music)
        {
            item.importObjectType = ImportObjectType::SFX;
            const std::string sfxStr = GetTextForImportObject(ImportObjectType::SFX);
            char sfxStrBuffer[256];
            strcpy(sfxStrBuffer, sfxStr.c_str());
            ListView_SetItemText(GetRenderViewHWND(), listItem, RenderViewSubitemID::WwiseImportObjectType, sfxStrBuffer);
        }
    }

}

void WAAPITransfer::RemoveRenderItemWwiseParent(RenderItemID renderId)
{
    auto foundIt = renderQueueItems.find(renderId);
    assert(foundIt != renderQueueItems.end());
    RemoveRenderItemWwiseParent(foundIt);
}

void WAAPITransfer::RemoveRenderItemWwiseParent(RenderItemMap::iterator it)
{
    RenderItem &renderItem = it->second.first;
    renderItem.wwiseGuid = "";

    char *noParentStr = "Not set.";
    auto mappedIndex = it->second.second;

    int listItem = ListView_MapIDToIndex(GetRenderViewHWND(), mappedIndex);
    ListView_SetItemText(GetRenderViewHWND(), listItem, RenderViewSubitemID::WwiseParent, noParentStr);
}

void WAAPITransfer::SetRenderItemOutputName(MappedListViewID mappedIndex, const std::string &newOutputName)
{
    auto filePath = m_renderListViewMap.find(mappedIndex);
    assert(filePath != m_renderListViewMap.end());

    RenderItem &item = GetRenderItemFromRenderItemId(filePath->second);
    item.outputFileName = newOutputName;

    char strbuff[WWISE_NAME_MAX_LEN];
    strcpy(strbuff, newOutputName.c_str());
    int listItem = ListView_MapIDToIndex(GetRenderViewHWND(), mappedIndex);
    ListView_SetItemText(GetRenderViewHWND(), listItem, 1, strbuff);
}


//Persistent statics
RenderItemMap WAAPITransfer::renderQueueItems = RenderItemMap{};
RenderProjectMap WAAPITransfer::renderQueueCachedProjects = RenderProjectMap{};

WwiseObjectMap WAAPITransfer::activeWwiseObjects = WwiseObjectMap{};

uint32 WAAPITransfer::RenderItemIdCounter = 0;

WAAPIImportOperation WAAPITransfer::lastImportOperation = WAAPIImportOperation::createNew;
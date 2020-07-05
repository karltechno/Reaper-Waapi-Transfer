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

WAAPITransfer::WAAPITransfer()
{
}

void WAAPITransfer::SetSelectedRenderParents(std::string const& wwiseObjGuid)
{
    bool isMusicSegment = GetWwiseObjectByGUID(wwiseObjGuid).isMusicContainer;

	ForEachSelectedRenderItem([this, &wwiseObjGuid, isMusicSegment](RenderItem& item)
	{
		SetRenderItemWwiseParent(item, wwiseObjGuid, isMusicSegment);
	});
}


void WAAPITransfer::SetSelectedImportObjectType(ImportObjectType typeToSet)
{
    bool musicTypeMismatch = false;

	ForEachSelectedRenderItem([&musicTypeMismatch, typeToSet, this](RenderItem& renderItem)
    {
        bool isParentMusicSegment = false;
        if (!renderItem.wwiseGuid.empty())
        {
            isParentMusicSegment = GetWwiseObjectByGUID(renderItem.wwiseGuid).isMusicContainer;
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
	});

    if (musicTypeMismatch)
    {
        SetStatusText("Music segments can only contain music objects.");
    }
}

void WAAPITransfer::SetSelectedDialogLanguage(int wwiseLanguageIndex)
{
    ForEachSelectedRenderItem([wwiseLanguageIndex,this](RenderItem& item) 
    {
		item.wwiseLanguageIndex = wwiseLanguageIndex;
    });
}

void WAAPITransfer::UpdateRenderQueue()
{
    auto projectFilesToAdd = GetRenderQueueProjectFiles();

    std::vector<RenderProjectMap::iterator> toDelete;

    for (auto iter = s_renderQueueCachedProjects.begin(), iterEnd = s_renderQueueCachedProjects.end();
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
        s_renderQueueCachedProjects.erase(iter);
    }

    //add render items that need to be added
    std::for_each(projectFilesToAdd.begin(), 
                  projectFilesToAdd.end(),
                  [this](const fs::path &path) 
                  {
                   auto iter = s_renderQueueCachedProjects.find(path.generic_string());

                   if (iter == s_renderQueueCachedProjects.end())
                   {
                       AddRenderItemsByProject(path);
                   }
                   });
}

RenderItem &WAAPITransfer::GetRenderItemFromRenderItemId(RenderItemID renderItemId)
{
    //maybe we should change to nested maps, oh well
    auto iter = s_renderQueueItems.find(renderItemId);
    assert(iter != s_renderQueueItems.end());
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
    auto foundIt = s_activeWwiseObjects.find(guid);
    assert(foundIt != s_activeWwiseObjects.end());
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
			m_connectedWwiseVersion = wwiseInfo["version"]["displayName"].GetVariant().GetString();
        }
    }

	m_connectionStatus = success;
    return success;
}

void WAAPITransfer::RunRenderQueueAndImport()
{
    //empty, no work to do
    if (s_renderQueueItems.empty())
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
    uint64_t numEmptyRenders = std::count_if(s_renderQueueItems.begin(), s_renderQueueItems.end(),
                                             [](const auto &item)
                                             { return item.second.first.wwiseGuid.empty(); });
    
    if (numEmptyRenders)
    {
        //nothing selected, just return
        if (numEmptyRenders == s_renderQueueItems.size())
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
    //OpenProgressWindow(hwnd, this);
}

void WAAPITransfer::SetStatusText(const std::string &status) const
{
    //SetWindowText(GetStatusTextHWND(), status.c_str());
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
        auto cachedGuid = s_activeWwiseObjects.find(wwiseObjectGuid);
        if (cachedGuid != s_activeWwiseObjects.end())
        {
            continue;
        }

        //check if type is valid for importing audio
        const std::string wwiseObjectType = result["type"].GetVariant().GetString();
        if (!IsParentContainer(wwiseObjectType))
        {
            continue;
        }

        WwiseObject wwiseNode;
        wwiseNode.type = wwiseObjectType;
        wwiseNode.path = result["path"].GetVariant().GetString();
        wwiseNode.name = result["name"].GetVariant().GetString();
		wwiseNode.guid = wwiseObjectGuid;
		wwiseNode.isMusicContainer = IsMusicContainer(wwiseObjectType);

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

        s_activeWwiseObjects.erase(treeIter->second);
        m_wwiseListViewMap.erase(treeIter);
    }
}

void WAAPITransfer::RemoveAllWwiseObjects()
{
    for (auto it = s_renderQueueItems.begin(), 
         endIt = s_renderQueueItems.end();
         it != endIt;
         ++it)
    {
        RemoveRenderItemWwiseParent(it);
    }

	s_activeWwiseObjects.clear();
    m_wwiseListViewMap.clear();
}



MappedListViewID WAAPITransfer::CreateWwiseObject(const std::string &guid, const WwiseObject &wwiseInfo)
{
    s_activeWwiseObjects.insert({ guid, wwiseInfo });
    
    return 0;
}


void WAAPITransfer::RemoveRenderItemsByProject(const fs::path &projectPath)
{
    auto iter = s_renderQueueCachedProjects.find(projectPath.generic_string());
    assert(iter != s_renderQueueCachedProjects.end());
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
		renderItem.renderItemId = s_RenderItemIdCounter++;
		s_renderQueueItems.insert({ renderItem.renderItemId, { renderItem, 0 } });
        renderIds.push_back(renderItem.renderItemId);
    }
    s_renderQueueCachedProjects.insert({ path.generic_string(), renderIds });
}

RenderItemMap::iterator WAAPITransfer::RemoveRenderItemFromList(RenderItemMap::iterator it)
{
    //remove from wwise parent in wwise object view
    const std::string& wwiseParentGuid = it->second.first.wwiseGuid;
    if (wwiseParentGuid != "")
    {
        GetWwiseObjectByGUID(wwiseParentGuid).renderChildren.erase(it->first);
    }

    //const HWND listView = GetRenderViewHWND();
    //int indexToRemove = ListView_MapIDToIndex(listView, it->second.second);
    //remove from listview 
    //ListView_DeleteItem(listView, indexToRemove);
    //remove from listview id map
    m_renderListViewMap.erase(it->second.second);
    //remove from render queue items
    return s_renderQueueItems.erase(it);
}

RenderItemMap::iterator WAAPITransfer::RemoveRenderItemFromList(uint32 renderItemId)
{
    auto iter = s_renderQueueItems.find(renderItemId);
    assert(iter != s_renderQueueItems.end());
    return RemoveRenderItemFromList(iter);
}


void WAAPITransfer::WaapiImportLoop()
{
    using namespace AK::WwiseAuthoringAPI;

    for (const auto &renderQueueItemPath : s_renderQueueCachedProjects)
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
    //PostMessage(hwnd, WM_TRANSFER_THREAD_MSG, 
    //            TRANSFER_THREAD_WPARAM::LAUNCH_RENDER_QUEUE_REQUEST, 0);

	CallOnReaperThread([](void*) {Main_OnCommand(41207, 1); }, nullptr);

    std::size_t totalRenderItems = s_renderQueueCachedProjects.size();
    uint32 numItemsProcessed = 0;
    bool renderQueueActive = true;
    bool allImportsSucceeded = true;
    std::vector<std::string> backupsToRestore;

    RenderProjectMap renderQueueProjectsCopy = s_renderQueueCachedProjects;

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
                    //PostMessage(hwnd, WM_TRANSFER_THREAD_MSG, TRANSFER_THREAD_WPARAM::IMPORT_SUCCESS, progressBarStep);

                    //success, delete backup
                    fs::remove(iter->first + RENDER_QUEUE_BACKUP_APPEND);

					//if we aren't copying then we should delete files in the reaper export folder
					if (!ShouldCopyToOriginals())
					{
						for (RenderItemID id : iter->second)
						{
							fs::remove(GetRenderItemFromRenderItemId(id).audioFilePath);
						}
					}

                    iter = renderQueueProjectsCopy.erase(iter);
                }
                else
                {
                    //inform main thread with new progress bar %
                    //PostMessage(hwnd, WM_TRANSFER_THREAD_MSG, TRANSFER_THREAD_WPARAM::IMPORT_FAIL, progressBarStep);

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
    //PostMessage(hwnd, WM_TRANSFER_THREAD_MSG, importWparam, 0);
}

bool WAAPITransfer::WaapiImportByProject(const std::string &projectPath, const std::string &RecallProjectPath)
{
    auto iter = s_renderQueueCachedProjects.find(projectPath);
    assert(iter != s_renderQueueCachedProjects.end());
    return WaapiImportByProject(iter, RecallProjectPath);
}


bool WAAPITransfer::WaapiImportByProject(RenderProjectMap::iterator projectIter, const std::string &RecallProjectPath)
{
    using namespace AK::WwiseAuthoringAPI;

    //wwise seems to crash importing a lot of items at once, so let's split it up into 10's for now
    const std::vector<uint32> &renderIdVec = projectIter->second;

    //we need separate arrays for each import operation
    AkJson::Array itemsCreateNew;
    AkJson::Array itemsUseExisting;
    AkJson::Array itemsReplaceExisting;

    itemsCreateNew.reserve(WAAPI_IMPORT_BATCH_SIZE);
    itemsUseExisting.reserve(WAAPI_IMPORT_BATCH_SIZE);
    itemsReplaceExisting.reserve(WAAPI_IMPORT_BATCH_SIZE);

    bool allSucceeded = true;
    uint32 const numBatches = static_cast<uint32>(std::ceilf(renderIdVec.size() / static_cast<float>(WAAPI_IMPORT_BATCH_SIZE)));

    for (uint32 batch = 0; batch < numBatches; ++batch)
    {
        itemsCreateNew.clear();
        itemsUseExisting.clear();
        itemsReplaceExisting.clear();

        uint32 const numItems = std::min(static_cast<uint32>(renderIdVec.size()) - batch * WAAPI_IMPORT_BATCH_SIZE, WAAPI_IMPORT_BATCH_SIZE);

        for (uint32 i = 0; i < numItems; ++i)
        {
            const RenderItem &renderItem = GetRenderItemFromRenderItemId(renderIdVec[i + (batch * WAAPI_IMPORT_BATCH_SIZE)]);
            
            //check if object has wwise GUID attached
            if (renderItem.wwiseGuid.empty())
            {
                continue;
            }

            AkJson importItem = AkJson(AkJson::Map{
                { "audioFile", AkVariant(renderItem.audioFilePath.generic_string()) },
                { "importLocation", AkVariant(renderItem.wwiseGuid) },
                { "objectType", AkVariant("Sound") }
            });

            auto &importMap = importItem.GetMap();

            switch (renderItem.importObjectType)
            {

            case ImportObjectType::SFX:
            {
                importMap.insert({ "objectPath", AkVariant("<Sound SFX>" + renderItem.outputFileName) });
				importMap.insert({ "audioSourceNotes", AkVariant(RecallProjectPath) });
            } break;

            case ImportObjectType::Voice:
            {
                importMap.insert({ "importLanguage", AkVariant(WwiseLanguages[renderItem.wwiseLanguageIndex]) });
                importMap.insert({ "objectPath", AkVariant("<Sound Voice>" + renderItem.outputFileName) });
				importMap.insert({ "audioSourceNotes", AkVariant(RecallProjectPath) });
            } break;

            case ImportObjectType::Music:
            {
                importMap.insert({ "objectPath", AkVariant("<Music Track>" + renderItem.outputFileName) });
            } break;

            }

			if (!renderItem.wwiseOriginalsSubpath.empty())
			{
				//TODO: Check for path correctness
				importMap.insert({ "originalsSubFolder", AkVariant(renderItem.wwiseOriginalsSubpath) });
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
    ForEachSelectedRenderItem([this, operation](RenderItem& renderItem)
    {
		renderItem.importOperation = operation;
    });
}

void WAAPITransfer::ForEachSelectedRenderItem(std::function<void(RenderItem&)> const& func) const
{
	for (auto it = s_renderQueueItems.begin(); it != s_renderQueueItems.end(); ++it)
	{
		RenderItem& renderItem = it->second.first;
		if (renderItem.isSelectedImGui)
		{
			func(renderItem);
		}
	}
}

void WAAPITransfer::SetRenderItemWwiseParent(RenderItem& item, const std::string &wwiseParentGuid, bool isMusicSegment)
{
    //remove previous render id from wwise object internal map
    if (!item.wwiseGuid.empty())
    {
        GetWwiseObjectByGUID(item.wwiseGuid).renderChildren.erase(item.renderItemId);
    }

    auto &newWwiseParent = GetWwiseObjectByGUID(wwiseParentGuid);
    newWwiseParent.renderChildren.insert(item.renderItemId);

    const std::string wwiseParentName = newWwiseParent.name;
    item.wwiseGuid = wwiseParentGuid;
    item.wwiseParentName = wwiseParentName;

    char strbuff[WWISE_NAME_MAX_LEN];
    strcpy(strbuff, wwiseParentName.c_str());

    //we need to change the import object type if the parent is set to a music track
    if (isMusicSegment)
    {
        item.importObjectType = ImportObjectType::Music;
    }
    else
    {
        //if it's already music type then we need to change it back, 
        //potentially could check somehow whether it should be dialog? - not high priority anyways..
        if (item.importObjectType == ImportObjectType::Music)
        {
            item.importObjectType = ImportObjectType::SFX;
        }
    }

}

void WAAPITransfer::RemoveRenderItemWwiseParent(RenderItemID renderId)
{
    auto foundIt = s_renderQueueItems.find(renderId);
    assert(foundIt != s_renderQueueItems.end());
    RemoveRenderItemWwiseParent(foundIt);
}

void WAAPITransfer::RemoveRenderItemWwiseParent(RenderItemMap::iterator it)
{
    RenderItem &renderItem = it->second.first;
    renderItem.wwiseGuid = "";

    char *noParentStr = "Not set.";
    auto mappedIndex = it->second.second;

    //int listItem = ListView_MapIDToIndex(GetRenderViewHWND(), mappedIndex);
    //ListView_SetItemText(GetRenderViewHWND(), listItem, RenderViewSubitemID::WwiseParent, noParentStr);
}

void WAAPITransfer::SetRenderItemOutputName(MappedListViewID mappedIndex, const std::string &newOutputName)
{
    auto filePath = m_renderListViewMap.find(mappedIndex);
    assert(filePath != m_renderListViewMap.end());

    RenderItem &item = GetRenderItemFromRenderItemId(filePath->second);
    item.outputFileName = newOutputName;

    char strbuff[WWISE_NAME_MAX_LEN];
    strcpy(strbuff, newOutputName.c_str());
    //int listItem = ListView_MapIDToIndex(GetRenderViewHWND(), mappedIndex);
    //ListView_SetItemText(GetRenderViewHWND(), listItem, 1, strbuff);
}


//Persistent statics
RenderItemMap WAAPITransfer::s_renderQueueItems = RenderItemMap{};
RenderProjectMap WAAPITransfer::s_renderQueueCachedProjects = RenderProjectMap{};

WwiseObjectMap WAAPITransfer::s_activeWwiseObjects = WwiseObjectMap{};

uint32 WAAPITransfer::s_RenderItemIdCounter = 0;

WAAPIImportOperation WAAPITransfer::lastImportOperation = WAAPIImportOperation::createNew;

std::unordered_set<std::string> WAAPITransfer::s_originalPathHistory = std::unordered_set<std::string>{};

bool WAAPITransfer::s_copyFilesToWwiseOriginals = true;
#pragma once
#include <Windows.h>
#include <Commctrl.h>
#include <AkAutobahn\Client.h>
#include <atomic>

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "RenderQueueReader.h"
#include "config.h"
#include "types.h"


class WAAPITransfer
{
public:
    WAAPITransfer(HWND window, int treeId, int statusTextid, int transferWindowId);
    ~WAAPITransfer() = default;

    WAAPITransfer(const WAAPITransfer&) = delete;
    WAAPITransfer &operator=(const WAAPITransfer&) = delete;

    friend class TransferSearch;

    //Column id's in render item listview
    enum RenderViewSubitemID
    {
        AudioFileName = 0,
        WwiseParent = 1,
        WwiseImportObjectType = 2,
        WwiseLanguage = 3,
        WaapiImportOperation = 4,
		WwiseOriginalsSubPath = 5
    };

    enum WwiseViewSubItemID
    {
        Name = 0,
        Path = 1,
    };

    enum class MultiSelectMode
    {
        NewSelection,
        FilterSelection,
        AddToSelection
    };

    //connect to wwise client and update status text
    bool Connect();

    //win32 helpers
    HWND GetStatusTextHWND()        const { return GetDlgItem(hwnd, m_statusTextId); }
    HWND GetWwiseObjectListHWND()   const { return GetDlgItem(hwnd, m_wwiseViewId); }
    HWND GetRenderViewHWND()        const { return GetDlgItem(hwnd, m_transferWindowId); }

    HWND GetProgressWindowHWND()    const { return m_progressWindow; }
    void SetProgressWindowHWND(HWND progressWindow) { m_progressWindow = progressWindow; }

    //Run render and import, called on main thread
    void RunRenderQueueAndImport();

    //Called when user presses 'Cancel' button whilst extension is importing
    void CancelTransferThread() { m_closeTransferThreadByUser = true; }

    //Set status message at bottom of window
    void SetStatusText(const std::string &status) const;

    //add objects selected in Wwise authoring app to the wwise object view
    void AddSelectedWwiseObjects();

    //remove wwise object from all maps and the tree view
    void RemoveWwiseObject(MappedListViewID toRemove);
    
    //removes all treeview objects and clears cache
    void RemoveAllWwiseObjects();

    //Call on window invocation to setup the list/tree views and recall state data into them
    void SetupAndRecreateWindow();

    //Checks the render queue to see if anything has been added or removed
    //updates necessary state and GUI - called on loop with WM_TIMER
    void UpdateRenderQueue();
    
    //sets all the selected list view items wwise parents
    void SetSelectedRenderParents(MappedListViewID wwiseTreeItem);

    //import as SFX, Music or dialog voice
    void SetSelectedImportObjectType(ImportObjectType type);

    //language dialog will be imported into wwise
    void SetSelectedDialogLanguage(int wwiseLanguageIndex);

    //If render item will create, replace or use existing parent
    void SetSelectedImportOperation(WAAPIImportOperation operation);

    //for each selected list view item apply function accepting a mapped list view id and listview index
    void ForEachSelectedRenderItem(std::function<void(MappedListViewID, uint32)> func) const;

    //Updates the Wwise parent that the render item will be imported into
    //if wwise parent is a music segment then the render items will have their import object type changed to match
    void SetRenderItemWwiseParent(MappedListViewID mappedIndex, const std::string &wwiseParentGuid, bool isMusicSegment = false);

    //used to reset render item wwise parent if user removes a wwise object from the internal list
    void RemoveRenderItemWwiseParent(RenderItemID renderId);
    void RemoveRenderItemWwiseParent(RenderItemMap::iterator it);

    //Updates the output name that will appear in wwise
    void SetRenderItemOutputName(MappedListViewID mappedIndex, const std::string &newOutputName);

    //Gets the render item associated with an render item ID
    RenderItem &GetRenderItemFromRenderItemId(RenderItemID renderItemId);

    //Gets the render item associated with an mapped listview ID
    RenderItem &GetRenderItemFromListviewId(MappedListViewID mappedListId);

    //Retrieves a wwise object by guid (from the internal active wwise objects view) 
    WwiseObject &GetWwiseObjectByGUID(const std::string &guid);

	bool ShouldCopyToOriginals() const { return s_copyFilesToWwiseOriginals; }
	void SetShouldCopyToOriginals(const bool shouldCopy) { s_copyFilesToWwiseOriginals = shouldCopy; }

    template<typename RenderIdIter>
    void WAAPITransfer::MultiSelect(RenderIdIter begin, RenderIdIter end, MultiSelectMode selectMode);

    //last import operation selected - thanks Tom!
    static WAAPIImportOperation lastImportOperation;

    //object owns this hwnd
    HWND hwnd;

	//recently entered wwise original subpaths
	static std::unordered_set<std::string> s_originalPathHistory;

private:
    //Window id's
    int m_statusTextId;
    int m_wwiseViewId;
    int m_transferWindowId;

    //The progress window will set and reset this value when it opens and closes
    std::atomic<HWND> m_progressWindow;

    std::atomic_bool m_closeTransferThreadByUser;

    //----------------------------------------------------------------
    //Static persistent 

    //Render queue items, key is audio file path
    //Value is the renderitem and then the mapped index in the listview 
    //(mapped index is used so we don't have to do a full search in m_renderListViewMap when deleting render items)
    static RenderItemMap s_renderQueueItems;
    static uint32 s_RenderItemIdCounter;

    //Stores the render queue project paths with a vector of renderitemid's
    static RenderProjectMap s_renderQueueCachedProjects;

    //working wwise objects stored in reaper project and between window invocation
    //wwise object GUID as key
    static WwiseObjectMap s_activeWwiseObjects;

	static bool s_copyFilesToWwiseOriginals;

    //----------------------------------------------------------------

    //Socket client for Waapi connection
    AK::WwiseAuthoringAPI::Client m_client;

    //Call this on window invocation to add cached wwise objects into tree view
    void RecreateWwiseView();

    //Call this on window invocation to add cached render queue objects into tree view
    void RecreateTransferListView();

    //Add a wwise object to treeview and internal data structures
    MappedListViewID CreateWwiseObject(const std::string &wwiseguid, const WwiseObject &wwiseInfo);
    MappedListViewID AddWwiseObjectToView(const std::string &guid, const WwiseObject &wwiseObject);

    //Remove all render items with a specific project path
    void RemoveRenderItemsByProject(const fs::path &projectPath);
    void RemoveRenderItemsByProject(RenderProjectMap::iterator it);

    //Add all render items with a specific project path
    void AddRenderItemsByProject(const fs::path &projectPath);

    //Adds a render item to list view and data structures
    //return is render id
    RenderItemID CreateRenderItem(const RenderItem &renderItem);
    MappedListViewID AddRenderItemToView(RenderItemID id, const RenderItem &renderitem);

    RenderItemMap::iterator RemoveRenderItemFromList(RenderItemMap::iterator it);
    RenderItemMap::iterator RemoveRenderItemFromList(uint32 renderItemId);

    //called async to check when a render queue has finished and import all the files
    void WaapiImportLoop();
    
    bool WaapiImportByProject(RenderProjectMap::iterator projectIter, const std::string &RecallProjectPath);
    bool WaapiImportByProject(const std::string &projectPath, const std::string &RecallProjectPath);

    //Map list view id to the wwise GUID;
    std::unordered_map<MappedListViewID, std::string> m_wwiseListViewMap;

    //Map render queue list item (with mapped index) to the render item id
    std::unordered_map<MappedListViewID, RenderItemID> m_renderListViewMap;
};


template<typename RenderIdIter>
inline void WAAPITransfer::MultiSelect(RenderIdIter begin, RenderIdIter end, MultiSelectMode selectMode)
{
        switch (selectMode)
        {
            case MultiSelectMode::NewSelection:
            {
                
            } break;
            case MultiSelectMode::FilterSelection:
            {

            } break;
            case MultiSelectMode::AddToSelection:
            {

            } break;
            default:
            {
            } break;
        }
}
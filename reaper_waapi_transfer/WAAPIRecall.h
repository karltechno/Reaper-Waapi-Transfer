#pragma once
#include <Windows.h>
#include <Commctrl.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <AkAutobahn/Client.h>

#include "config.h"
#include "types.h"

class WAAPIRecall
{
public:
    WAAPIRecall(HWND hwnd, int recallListId, int statusTextId);

    WAAPIRecall(const WAAPIRecall&) = delete;
    WAAPIRecall &operator=(const WAAPIRecall&) = delete;

    ~WAAPIRecall() = default;

    struct RecallItem
    {
        std::string wwiseGuid;
        std::string wwiseName;
        std::string projectPath;
    };

    //waapi connect
    bool Connect();

    void SetupWindow();

    void UpdateWwiseObjects();

    void OpenSelectedProject();

    RecallItem &GetRecallItem(uint32 mappedId);

    const HWND GetRecallListHWND() { return GetDlgItem(hwnd, m_recallListId); }
    const HWND GetStatusTextHWND() { return GetDlgItem(hwnd, m_statusTextId); }

    void SetStatusText(const std::string &status);

    //Column id's in listview
    enum SubitemID
    {
        WwiseObjectName = 0,
        ReaProjectName = 1,
        ReaProjectPath = 2
    };

    using RecallMap = std::unordered_map<uint32, RecallItem>;

    HWND hwnd;

private:
    int m_recallListId;
    int m_statusTextId;

    void AddRecallItem(const RecallItem &item);

    RecallMap::iterator RemoveRecallItem(const std::string &wwiseGuid);
    RecallMap::iterator RemoveRecallItem(uint32 mappedId);
    RecallMap::iterator RemoveRecallItem(RecallMap::iterator iter);

    AK::WwiseAuthoringAPI::Client m_client;

    RecallMap m_mappedIdToRecallObject;

    //wwise guids selected so we can easily remove duplicates
    std::unordered_set<std::string> m_cachedGuids;

    std::string m_currentStatusText;
};


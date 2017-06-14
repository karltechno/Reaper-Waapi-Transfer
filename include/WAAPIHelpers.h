#pragma once
#include <Windows.h>
#include <Commctrl.h>

#include <string>
#include <unordered_map>
#include <initializer_list>

#include <AK\WwiseAuthoringAPI\waapi.h>
#include <AkAutobahn\Client.h>

#include "config.h"


inline bool IsParentType(const std::string &wwiseType)
{
    return WwiseParentTypes.find(wwiseType) != WwiseParentTypes.end();
}

bool GetAllSelectedWwiseObjects(AK::WwiseAuthoringAPI::AkJson &resultsMap, 
                                AK::WwiseAuthoringAPI::Client &client, 
                                bool getNotes = false);

inline bool GetWaapiResultsArray(AK::WwiseAuthoringAPI::AkJson::Array &arrayIn, 
                                 AK::WwiseAuthoringAPI::AkJson &results)
{
    using namespace AK::WwiseAuthoringAPI;
    switch (results.GetType())
    {
    case AkJson::Type::Map:
    {
        if (results.HasKey("objects"))
        {
            arrayIn = results["objects"].GetArray();
            return true;
        }
        else if (results.HasKey("return"))
        {
            arrayIn = results["return"].GetArray();
            return true;
        }
        else
        {
            assert(!"Not implemented.");
        }

    } break;


    default:
        assert(!"Not implemented.");
        return false;
    }

    return false;
}

inline const std::string &GetResultsErrorMessage(AK::WwiseAuthoringAPI::AkJson &resultsIn)
{
    return resultsIn.GetMap()["message"].GetVariant().GetString();
}

bool GetChildren(const AK::WwiseAuthoringAPI::AkVariant &path,
                 AK::WwiseAuthoringAPI::AkJson &resultsIn,
                 AK::WwiseAuthoringAPI::Client &client,
                 bool getNotes = false);


class WwiseImageList
{
public:
    static void LoadIcons(std::initializer_list<std::pair<std::string, int>> icons);
    static int GetIconForWwiseType(const std::string wwiseType);
    static HIMAGELIST GetImageList() { return imageList; }
private:
    static std::unordered_map<std::string, int> iconList;
    static HIMAGELIST imageList;
};

enum class WAAPIImportOperation
{
    createNew,
    useExisting,
    replaceExisting
};

inline std::string GetImportOperationString(WAAPIImportOperation operation)
{
    switch (operation)
    {
    case WAAPIImportOperation::createNew:
        return std::string("createNew");
        break;
    case WAAPIImportOperation::useExisting:
        return std::string("useExisting");
        break;
    case WAAPIImportOperation::replaceExisting:
        return std::string("replaceExisting");
        break;
    default:
        return std::string();
        break;
    }
}

bool WaapiImportItems(const AK::WwiseAuthoringAPI::AkJson::Array &items,
                      AK::WwiseAuthoringAPI::Client &client,
                      WAAPIImportOperation importOperation);
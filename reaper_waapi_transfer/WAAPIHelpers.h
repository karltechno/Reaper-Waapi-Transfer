#pragma once
#include <Windows.h>
#include <Commctrl.h>

#include <string>
#include <unordered_map>
#include <initializer_list>

#include <AK/WwiseAuthoringAPI/waapi.h>
#include <AK/WwiseAuthoringAPI/AkAutobahn/Client.h>

#include "config.h"
#include "types.h"


//////////////////////////////////////////////////////////////////////////
//Helpers calling Waapi


//get items selected in wwise authoring application, return if waapi call was successful
bool GetAllSelectedWwiseObjects(AK::WwiseAuthoringAPI::AkJson &resultsOut, 
                                AK::WwiseAuthoringAPI::Client &client, 
                                bool getNotes = false);


//get children of a given waapi path, returns if waapi call was successful
bool GetChildren(const AK::WwiseAuthoringAPI::AkVariant &path,
                 AK::WwiseAuthoringAPI::AkJson &resultsOut,
                 AK::WwiseAuthoringAPI::Client &client,
                 bool getNotes = false);

//get the array for a succesfull call to any of the above functions, results is 'resultsOut' from above functions
void GetWaapiResultsArray(AK::WwiseAuthoringAPI::AkJson::Array &arrayIn,
                          AK::WwiseAuthoringAPI::AkJson &results);


//get the error message for an unsuccessful waapi call to the above functions
inline const std::string &GetResultsErrorMessage(AK::WwiseAuthoringAPI::AkJson &resultsIn)
{
    return resultsIn.GetMap()["message"].GetVariant().GetString();
}


//Import given items, returns if waapi call was successful
bool WaapiImportItems(const AK::WwiseAuthoringAPI::AkJson::Array &items,
                      AK::WwiseAuthoringAPI::Client &client,
                      WAAPIImportOperation importOperation,
                      bool autoAddToSourceControl);

//////////////////////////////////////////////////////////////////////////


//Check if type is usable for importing render items into
bool IsParentContainer(const std::string &wwiseType);

// Returns if the given wwise container is a music container.
bool IsMusicContainer(const std::string &wwiseType);


//Holds win32 image list and map of wwise types to their image ID in the list
class WwiseImageList
{
public:
    static void LoadIcons(std::initializer_list<std::pair<std::string, int>> icons);
    static int GetIconForWwiseType(const std::string wwiseType);
    static HIMAGELIST GetImageList() { return s_imageList; }
private:
    static std::unordered_map<std::string, int> s_iconList;
    static HIMAGELIST s_imageList;
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



//http://the-witness.net/news/2012/11/scopeexit-in-c11/
template <typename F>
struct ScopeExit
{
    ScopeExit(F f) : f(f) {}
    ~ScopeExit() { f(); }
    F f;
};

template <typename F>
ScopeExit<F> MakeScopeExit(F f)
{
    return ScopeExit<F>(f);
};
#define STRING_JOIN2(arg1, arg2) DO_STRING_JOIN2(arg1, arg2)
#define DO_STRING_JOIN2(arg1, arg2) arg1 ## arg2
#define SCOPE_EXIT(code) \
    auto STRING_JOIN2(scope_exit_, __LINE__) = MakeScopeExit([=](){code;})


//https://herbsutter.com/2009/10/18/mailbag-shutting-up-compiler-warnings/
template<class T> void ignore(const T&) {}

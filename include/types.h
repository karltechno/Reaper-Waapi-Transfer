#pragma once
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <string>

using uint32 = std::uint32_t;
using uint16 = std::uint16_t;

using int32 = std::int32_t;
using int16 = std::int16_t;

//TODO: sort this out with CMake
#define FILESYSTEM_EXPERIMENTAL

#ifdef FILESYSTEM_EXPERIMENTAL
#include <filesystem>
namespace fs = std::experimental::filesystem;
#endif
#ifdef FILESYSTEM_NATIVE
#include <filesystem> 
namespace fs = std::filesystem;
#endif
#ifdef FILESYSTEM_BOOST
namespace fs = boost::filesystem;
#endif

using MappedListViewID = int32;
using RenderItemID = uint32;

struct WwiseObject
{
    std::string name;
    std::string type;
    std::string path;
    std::unordered_set<RenderItemID> renderChildren;

    //Currently 'type' is just used for the icon decoration, only music segment changes
    //functionality as it dictates what object type a render child of a wwise object can be
    bool isMusicSegment;
};

enum class ImportObjectType
{
    SFX,
    Voice,
    Music
};

enum class WAAPIImportOperation
{
    createNew,
    useExisting,
    replaceExisting
};

struct RenderItem
{
    fs::path projectPath;
    fs::path audioFilePath;
    std::string outputFileName;

    std::string wwiseGuid;
    std::string wwiseParentName;
	std::string wwiseOriginalsSubpath;

    WAAPIImportOperation importOperation;
    ImportObjectType importObjectType;

    int wwiseLanguageIndex;

    //info for searching
    uint32 regionRenderFlags;

    //reaper region info
    int reaperRegionId;
    int regionMatrixOffset;

    //optional based on flags
    std::string trackStemGuid;
    double inTime, outTime;
};

using RenderItemMap = std::unordered_map<RenderItemID, std::pair<RenderItem, MappedListViewID>>;
using RenderProjectMap = std::unordered_map<std::string, std::vector<RenderItemID>>;
using WwiseObjectMap = std::unordered_map<std::string, WwiseObject>;

#pragma once
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <assert.h>

using uint32 = std::uint32_t;
using uint16 = std::uint16_t;

using int32 = std::int32_t;
using int16 = std::int16_t;

// Todo: CMake Magic?
#define FILESYSTEM_EXPERIMENTAL (0)
#define FILESYSTEM_NATIVE (0)

#define HAS_CPP17 __cplusplus >= 201402L

#ifdef _MSC_VER
	#if _MSC_VER >= 1914 && HAS_CPP17
		#undef FILESYSTEM_NATIVE
		#define FILESYSTEM_NATIVE (1)
		// std::filesystem came in update 15.7
	#else
		#undef FILESYSTEM_EXPERIMENTAL
		#define FILESYSTEM_EXPERIMENTAL (1)
	#endif
#endif


#if FILESYSTEM_EXPERIMENTAL
	#include <filesystem>
	namespace fs = std::experimental::filesystem;
#elif FILESYSTEM_NATIVE
	#include <filesystem> 
	namespace fs = std::filesystem;
#elif FILESYSTEM_BOOST
	namespace fs = boost::filesystem;
#elif
	#error Could not find a std::filesystem implementation!
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
    bool isMusicContainer;

	bool isSelectedImGui = false;
};

enum class ImportObjectType
{
    Music,
    SFX,
    Voice
};

enum class WAAPIImportOperation
{
    createNew,
    replaceExisting,
    useExisting
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

	bool isSelectedImGui = false;
};

using RenderItemMap = std::unordered_map<RenderItemID, std::pair<RenderItem, MappedListViewID>>;
using RenderProjectMap = std::unordered_map<std::string, std::vector<RenderItemID>>;
using WwiseObjectMap = std::unordered_map<std::string, WwiseObject>;

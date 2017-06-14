#pragma once
#include <string>
#include <vector>

#include "config.h"
#include "WAAPIHelpers.h"
const std::string QueuedRenderPrjText("QUEUED_RENDER_OUTFILE");

enum class ImportObjectType
{
    SFX,
    Voice,
    Music
};

std::string GetTextForImportObject(ImportObjectType importObject);


enum RenderItemFlags
{
    //bounds
    RenderBoundsRegion = 1 << 0,
    RenderBoundsCustom = 1 << 1,
     
    //source
    RenderSourceSelectedStems   = 1 << 2,
    RenderSourceRegionMatrix    = 1 << 3,
    RenderSourceMaster          = 1 << 4,
    RenderSourceSelectedMedia   = 1 << 5,
    
    RenderMatrixSourceAllTracks         = 1 << 6,
    RenderMatrixSourceSelectedTrack     = 1 << 7
};

struct RenderItem
{
    fs::path projectPath;
    fs::path audioFilePath;
    std::string outputFileName;

    std::string wwiseGuid;
    std::string wwiseParentName;

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


fs::path GetRenderQueueDir();

std::vector<RenderItem> ParseRenderQueueFile(const fs::path &path);

//in progress
void ParseRenderQueue(const fs::path &path);

std::vector<fs::path> GetRenderQueueProjectFiles();


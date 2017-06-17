#pragma once
#include <string>
#include <vector>

#include "config.h"
#include "types.h"
#include "WAAPIHelpers.h"

const std::string QueuedRenderPrjText("QUEUED_RENDER_OUTFILE");


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

fs::path GetRenderQueueDir();

std::vector<RenderItem> ParseRenderQueueFile(const fs::path &path);

std::vector<RenderItem> ParseRenderQueue(const fs::path &path);

std::vector<fs::path> GetRenderQueueProjectFiles();
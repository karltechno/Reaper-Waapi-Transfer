#include <fstream>
#include <sstream>

#include "reaper_plugin_functions.h"

#include "RenderQueueReader.h"
#include "types.h"

/*
 Info for parsing render queue item infomation from RPP files

 - RENDER_RANGE
    - first number indicates 0 based index of the selected render bounds option
    - 0 = Custom Time Range
    - 1 = Entire Project
    - 2 = Time Selection
    - 3 = Project Regions


 - RENDER_STEMS
    - Based off whats selected in 'Source' render option
    - 0 = Master Mix
    - 1 = Master Mix + Stems
    - 3 = Stems: Selected Tracks
    - 8 = Render Region Matrix
    - 32 = Selected Media Items


 - Regions:
             region id    time (dont care for now)     name    RenderSettingsId     Not sure what the rest of this is, doesn't seem relevant ATM.
      MARKER     2        6.67291127704084            "NAME"       5               0          1       R



      RenderSettingsID:
        - 0 = normal markers (dont care for now)
        - 1 = Tracks listed below under <REGIONRENDER
        - 4 = all tracks 
        - 5 = Master mix + tracks below
        - 7 = Master mix + all tracks
*/

//NOTE: This is not a fully featured RPP parser, just implements enough to get needed infomation 
//for searching render items by region and track 

//This parser isn't implemented yet --

enum ReaperRegionRenderFlags
{
    RegionSelectedTracks    = 1 << 0,
    RegionAllTracks         = 1 << 1,
    RegionMasterMix         = 1 << 2
};

enum class ReaperRenderRangeMode
{
    CustomTimeRange = 0,
    EntireProject   = 1,
    TimeSelection   = 2,
    ProjectRegions  = 3
};



struct ReaperRegion
{
    std::string note;
    std::vector<std::string> regionRenderTracks;
    uint32 id;
    uint32 regionRenderFlags{};
};

struct ReaperTrack
{
    std::string name;
    std::string guid;
    bool isSelected;
};

struct ReaperRenderInfo
{
    ReaperRenderRangeMode rangeMode;
    uint32 stemFlags;

    std::vector<ReaperTrack> tracks;
    std::vector<ReaperRegion> regions;
};

std::string GetStringToken(std::stringstream &line)
{
    std::string text;
    std::ios_base::fmtflags prevSkipWs = line.flags() & std::ios_base::skipws;
    
    std::skipws(line);

    //work out if its a quoted string
    char endchar{};
    char c;
    line >> c;
    if (c == '\"')
    {
        endchar = '\"';
    }
    else
    {
        endchar = ' ';
        text.push_back(c);
    }

    std::noskipws(line);
    //add rest of chars
    while (line >> c)
    {
        if (c != endchar)
        {
            text.push_back(c);
        }
        else
        {
            break;
        }
    }
    prevSkipWs ? std::skipws(line) : std::noskipws(line);
    return text;
}

ReaperRegion &ParseRegion(std::ifstream &ifstream, ReaperRegion &region)
{
    std::string line;
    std::getline(ifstream, line);
    std::stringstream markerStream(line);
    std::skipws(markerStream);

    std::string markerToken;
    markerStream >> markerToken;
    if (markerToken == "MARKER")
    {
        //marker finishes here 
    }
    else if (markerToken == "<REGIONRENDER")
    {
        //get region render tracks
        while (std::getline(ifstream, line))
        {
            std::stringstream trackStream(line);
            std::skipws(trackStream);
            trackStream >> markerToken;
            if (markerToken[0] == '>')
            {
                //we are done - eat next "MARKER" line (signifies end of region)
                std::getline(ifstream, markerToken);
                break;
            }
            if (markerToken == "TRACK")
            {
                //get track GUID
                trackStream >> markerToken;
                region.regionRenderTracks.push_back(markerToken);
            }
        }
    } 
    return region;
}

ReaperTrack &ParseTrack(std::ifstream &fstream, ReaperTrack &track)
{
    std::string line;
    std::ios_base::fmtflags prevSkipWs = fstream.flags() & std::ios_base::skipws;
    std::skipws(fstream);
    int subElementDepth = 0;
    while (std::getline(fstream, line))
    {
        std::stringstream lineStream(line);
        std::skipws(lineStream);
        std::string firstToken;
        lineStream >> firstToken;

        if (firstToken[0] == '<')
        {
            ++subElementDepth;
        }

        if (firstToken[0] == '>')
        {
            if (subElementDepth)
            {
                --subElementDepth;
            }
            else
            {
                //we are done
                break;
            }
        }

        if (!subElementDepth)
        {
            if (firstToken == "SEL")
            {
                int selected;
                lineStream >> selected;
                track.isSelected = selected;
                continue;
            }

            if (firstToken == "NAME")
            {
                track.name = GetStringToken(lineStream);
                continue;
            }
        }
    }
    prevSkipWs ? std::skipws(fstream) : std::noskipws(fstream);
    return track;
}

void AddRenderInfo(const std::vector<ReaperTrack> &tracks, 
                   const std::vector<ReaperRegion> &regions, 
                   std::vector<RenderItem> &renderItems,
                   ReaperRenderRangeMode projectRangeMode,
                   uint32 projectRenderSourceFlags)
{
    //check size every time we increment incase project is malformed
    uint32 renderItemCount = 0;
    if (projectRenderSourceFlags & RenderSourceRegionMatrix)
    {
        //render matrix
        for (const ReaperRegion &region : regions)
        {
            //per region render master flag
            int matrixOffsetCounter = 0;
            if (region.regionRenderFlags & RenderSourceMaster)
            {
                //add master
                renderItems[renderItemCount].regionRenderFlags = RenderItemFlags::RenderSourceMaster
                                                               | RenderItemFlags::RenderBoundsRegion;

                renderItems[renderItemCount].reaperRegionId = region.id;
                renderItems[renderItemCount].regionMatrixOffset = matrixOffsetCounter++;
                if (++renderItemCount >= renderItems.size()) return;
            }
            if (region.regionRenderFlags & RenderItemFlags::RenderMatrixSourceAllTracks)
            {
                for (const ReaperTrack &track : tracks)
                {
                    renderItems[renderItemCount].regionRenderFlags = RenderItemFlags::RenderSourceSelectedStems
                                                                   | RenderItemFlags::RenderBoundsRegion;

                    renderItems[renderItemCount].trackStemGuid = track.guid;
                    renderItems[renderItemCount].reaperRegionId = region.id;
                    renderItems[renderItemCount].regionMatrixOffset = matrixOffsetCounter++;
                    if (++renderItemCount >= renderItems.size()) return;
                }
            }
            else
            {
                for (const std::string &matrixTrack : region.regionRenderTracks)
                {
                    //add matrix tracks
                    renderItems[renderItemCount].regionRenderFlags = RenderItemFlags::RenderSourceRegionMatrix
                                                                   | RenderItemFlags::RenderBoundsRegion;
                    renderItems[renderItemCount].reaperRegionId = region.id;
                    renderItems[renderItemCount].regionMatrixOffset = matrixOffsetCounter++;
                    renderItems[renderItemCount].trackStemGuid = std::move(matrixTrack);
                    if (++renderItemCount >= renderItems.size()) return;
                }
            }
        }
    }
    else if (projectRenderSourceFlags & RenderSourceSelectedStems)
    {
        switch (projectRangeMode)
        {
        case ReaperRenderRangeMode::CustomTimeRange:
        case ReaperRenderRangeMode::EntireProject:
        case ReaperRenderRangeMode::TimeSelection:
        {
            if (projectRenderSourceFlags & RenderSourceMaster)
            {
                //add master
                //TODO: add time range
                renderItems[renderItemCount].regionRenderFlags = RenderItemFlags::RenderSourceMaster
                                                                | RenderItemFlags::RenderBoundsCustom;
                if (++renderItemCount >= renderItems.size()) return;
            }

            for (const ReaperTrack &track : tracks)
            {
                //add track
                renderItems[renderItemCount].trackStemGuid = track.guid;
                renderItems[renderItemCount].regionRenderFlags = RenderItemFlags::RenderBoundsCustom 
                                                               | RenderItemFlags::RenderSourceSelectedStems;
                if (++renderItemCount >= renderItems.size()) return;
            }
        } break;
        case ReaperRenderRangeMode::ProjectRegions:
        {
            for (const ReaperRegion &region : regions)
            {
                if (projectRenderSourceFlags & RenderSourceMaster)
                {
                    //add master
                    renderItems[renderItemCount].regionRenderFlags = RenderItemFlags::RenderSourceMaster
                                                                   | RenderItemFlags::RenderBoundsRegion;

                    renderItems[renderItemCount].reaperRegionId = region.id;
                    renderItems[renderItemCount].regionMatrixOffset = 0;
                    if (++renderItemCount >= renderItems.size()) return;
                }

                for (const ReaperTrack &track : tracks)
                {
                    //add track
                    renderItems[renderItemCount].trackStemGuid = std::move(track.guid);
                    renderItems[renderItemCount].regionRenderFlags = RenderItemFlags::RenderSourceSelectedStems
                                                                   | RenderItemFlags::RenderBoundsRegion;

                    renderItems[renderItemCount].reaperRegionId = region.id;
                    renderItems[renderItemCount].regionMatrixOffset = 0;
                    if (++renderItemCount >= renderItems.size()) return;
                }
            }
        } break;
        default:
        {
            assert(!"Unidentified project render range.");
        } break;
        }
    }
}

std::vector<RenderItem> ParseRenderQueue(const fs::path &path)
{
    std::vector<ReaperTrack> selectedTracks;
    std::vector<ReaperRegion> regions;
    std::vector<RenderItem> renderItems;
    ReaperRenderRangeMode projectRangeMode{};

    uint32 projectRenderSourceFlags{};

    const std::string projectPath = path.generic_string();
    std::ifstream fileReader(path);

    if (!fileReader.is_open())
    {
        return renderItems;
    }

    std::string line;

    while (std::getline(fileReader, line))
    {
        std::stringstream lineStream(line);
        std::skipws(lineStream);
        std::string tokenName;
        lineStream >> tokenName;

        if (tokenName == "QUEUED_RENDER_OUTFILE")
        {
            RenderItem item;
            item.audioFilePath = GetStringToken(lineStream);
            item.wwiseParentName = "Not set.";
            item.projectPath = projectPath;
            item.outputFileName = item.audioFilePath.filename().replace_extension("").generic_string();
            item.importOperation = WAAPIImportOperation::createNew;
            item.importObjectType = ImportObjectType::SFX;
            item.wwiseLanguageIndex = 0;
            renderItems.push_back(item);

            continue;
        }

        if (tokenName == "RENDER_RANGE")
        {
            int rangeId;
            lineStream >> rangeId;
            projectRangeMode = static_cast<ReaperRenderRangeMode>(rangeId);
        }

        if (tokenName == "RENDER_STEMS")
        {
            int stemsId;
            lineStream >> stemsId;
            switch (stemsId)
            {
            case 0:
                //master mix
                projectRenderSourceFlags = RenderSourceMaster;
                break;

            case 1:
                //master mix and stems
                projectRenderSourceFlags = RenderSourceMaster | RenderSourceSelectedStems;
                break;

            case 3:
                //selected tracks
                projectRenderSourceFlags = RenderSourceSelectedStems;
                break;

            case 8:
                //region matrix 
                projectRenderSourceFlags = RenderSourceRegionMatrix;
                break;

            case 32:
                //selected media items
                projectRenderSourceFlags = RenderSourceSelectedMedia;
                break;

            default:
                assert(!"Unidentified stem render mode");
                break;
            }
            continue;
        }

        if (tokenName == "<TRACK")
        {
            //get track guid
            ReaperTrack track;
            lineStream >> track.guid;
            ParseTrack(fileReader, track);
            selectedTracks.push_back(std::move(track));
        }

        if (tokenName == "MARKER")
        {
            int markerId;
            lineStream >> markerId;
            std::string name;

            //eat the marker time
            lineStream >> name;
            name = GetStringToken(lineStream);

            int markerRenderMode;
            lineStream >> markerRenderMode;
            if (!markerRenderMode)
            {
                //marker not a region
                continue;
            }
            ReaperRegion region;
            if      (markerRenderMode == 1) region.regionRenderFlags = RenderItemFlags::RenderSourceSelectedStems;
            else if (markerRenderMode == 4) region.regionRenderFlags = RenderItemFlags::RenderMatrixSourceAllTracks;
            else if (markerRenderMode == 5) region.regionRenderFlags = RenderItemFlags::RenderSourceSelectedStems   | RenderItemFlags::RenderSourceMaster;
            else if (markerRenderMode == 7) region.regionRenderFlags = RenderItemFlags::RenderMatrixSourceAllTracks | RenderItemFlags::RenderSourceMaster;

            region.id = markerId;
            region.note = name;
            ParseRegion(fileReader, region);
            regions.push_back(std::move(region));
        }
    }

    AddRenderInfo(selectedTracks, regions, renderItems, projectRangeMode, projectRenderSourceFlags);
    return renderItems;
}


#if 0
//simple parser just for render item path before we implement searching
std::vector<RenderItem> ParseRenderQueueFile(const fs::path &path)
{
    std::vector<RenderItem> items;
    const std::string projectPath = path.generic_string();
    std::ifstream fileReader(path);

    if (!fileReader.is_open())
    {
        return items;
    }

    std::string line;
    //eat first line
    std::getline(fileReader, line);

    while (std::getline(fileReader, line))
    {
        if (std::size_t strPos = line.find(QueuedRenderPrjText) != line.npos)
        {
            RenderItem item;
            //Find start of the file path in the line
            std::size_t pathStart = line.find_first_of('\"');
            line = line.substr(pathStart + 1, line.find_last_of('"') - pathStart - 1);
            item.audioFilePath = line;
            item.wwiseParentName = "Not set.";
            item.projectPath = projectPath;
            item.audioFilePath.filename();
            item.outputFileName = item.audioFilePath.filename().replace_extension("").generic_string();
            item.importOperation = WAAPIImportOperation::createNew;
            item.importObjectType = ImportObjectType::SFX;
            item.wwiseLanguageIndex = 0;

            items.push_back(item);
        }
        else
        {
            //there aren't any more files after this
            break;
        }
    }

    return items;
}
#endif

std::vector<fs::path> GetRenderQueueProjectFiles()
{
    fs::directory_iterator dirIter(GetRenderQueueDir());
    std::vector<fs::path> paths;
    char reaperProjectName[256];
    GetProjectName(EnumProjects(-1, nullptr, 0), reaperProjectName, 256);
    for (const auto& iter : dirIter)
    {
        if (!fs::is_regular_file(iter))
        {
            continue;
        }
        const std::string filename = iter.path().filename().generic_string();
        
        //TODO: this isn't the most robust way to check, refactor later..
        if (filename.find("qrender") == filename.npos)
        {
            continue;
        }

        //if reaper crashed the .wpbak files might still be here, delete them if and continue if so
        //check only at the end of the file incase the appended sequence is in the project name
        const size_t extensionOffset = filename.length() - (RENDER_QUEUE_BACKUP_APPEND.length() + 1);
        if (filename.find(RENDER_QUEUE_BACKUP_APPEND, extensionOffset) != filename.npos)
        {
            fs::remove(iter);
            continue;
        }
        else if(iter.path().extension() == ".rpp")
        {
            paths.push_back(iter.path());
        }
    }
    return paths;
}


fs::path GetRenderQueueDir()
{
    fs::path path(GetResourcePath());
    path.append("QueuedRenders");
    if (!fs::exists(path))
    {
        fs::create_directory(path);
    }

    return path;
}

char const* GetTextForImportObject(ImportObjectType importObject)
{
    switch (importObject)
    {

    case ImportObjectType::SFX:
    {
        return "SFX";
    } break;

    case ImportObjectType::Music:
    {
        return "Music";
    } break;

    case ImportObjectType::Voice:
    {
        return "Dialog";
    } break;

    default:
    {
		return "";
    } break;

    }
}
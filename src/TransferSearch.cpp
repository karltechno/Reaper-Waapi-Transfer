#include <algorithm>
#include <cctype>

#include "reaper_plugin_functions.h"

#include "TransferSearch.h"

void TransferSearch::UpdateReaperState()
{
    //enumerate tracks
    int i = 0;
    for (auto track = GetTrack(nullptr, i); 
         track; 
         ++i)
    {
        char name[256];
        GetSetMediaTrackInfo_String(track, "P_NAME", name, false);

        //name -> tolower for searching, can cause collisions with two names capitalized differently but thats an odd use case
        //user can still select track and match GUID anyway.
        std::transform(std::begin(name), std::end(name), std::begin(name), [](char c) {return std::tolower(c); });

        char guidStr[64];
        guidToString(GetTrackGUID(track), guidStr);
    }
}

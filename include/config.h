#pragma once
#include <string>
#include <unordered_set>

#include "types.h"

//https://www.audiokinetic.com/library/edge/?source=Help&id=understanding_naming_conventions_in_wwise
constexpr uint32 WWISE_NAME_MAX_LEN = 260;

const std::string PROJ_NOTE_PREFIX = "REAPROJ:";
const std::string RENDER_QUEUE_BACKUP_APPEND = ".wpbak";

constexpr int WAAPI_CLIENT_TIMEOUT_MS = 2000;

constexpr uint32 WAAPI_IMPORT_BATCH_SIZE = 10;
constexpr int WAAPI_DEFAULT_PORT = 8080;

#define WT_VERSION 0x000107

#define WT_VERSION_MAJOR ((WT_VERSION & 0xFF0000) >> 16)
#define WT_VERSION_MINOR ((WT_VERSION & 0x00FF00) >> 8)
#define WT_VERSION_INCREMENTAL (WT_VERSION & 0x0000FF)

static const char *WwiseLanguages[] =
{
    "English(US)",
    "English(UK)",
    "Arabic",
    "Bulgarian",
    "Chinese(HK)",
    "Chinese(PRC)",
    "Chinese(Taiwan)",
    "Czech",
    "Danish",
    "Dutch",
    "English(Australia)",
    "English(India)",
    "Finnish",
    "French(Canada)",
    "French(France)",
    "German",
    "Greek",
    "Hebrew",
    "Hungarian",
    "Indonesian",
    "Italian",
    "Japanese",
    "Korean",
    "Latin",
    "Norwegian",
    "Polish",
    "Portuguese(Brazil)",
    "Portuguese(Portugal)",
    "Romanian",
    "Russian",
    "Slovenian",
    "Spanish(Mexico)",
    "Spanish(Spain)",
    "Spanish(US)",
    "Swedish",
    "Turkish",
    "Ukrainian",
    "Vietnamese"
};


static const std::unordered_set<std::string> WwiseParentTypes
{
    "Folder",
    "WorkUnit",
    "RandomSequenceContainer",
    "BlendContainer",
    "ActorMixer",
    "SwitchContainer",
    "MusicSegment"
};


#pragma once
#include <Windows.h>

#include "reaper_plugin.h"

//globals
extern HWND g_parentWindow;
extern REAPER_PLUGIN_HINSTANCE g_hInst;
extern int g_Waapi_Port;

bool HookCommandProc(int command, int flag);


using CallOnReaperFn = void(*)(void*);
// Async call on reaper thread.
void CallOnReaperThread(CallOnReaperFn fn, void* ctx);
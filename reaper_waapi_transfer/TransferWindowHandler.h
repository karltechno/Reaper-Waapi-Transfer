#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h>

#define WM_TRANSFER_THREAD_MSG (WM_USER + 1)
#define WM_PROGRESS_WINDOW_MSG (WM_USER + 2)
#define WM_TRANSFER_SELECT_ALL (WM_USER + 3)

LRESULT TransferWindow_ReaperKeyboardHook(int code, WPARAM wParam, LPARAM lParam);

enum TRANSFER_THREAD_WPARAM : WPARAM
{
    //all wwise imports succeeded
    THREAD_EXIT_SUCCESS,

    //Indiciates at least 1 wwise import failed
    THREAD_EXIT_FAIL,

    //user cancelled the thread prematurely
    THREAD_EXIT_BY_USER,

    //Transfer thread requests main thread to start render queue
    LAUNCH_RENDER_QUEUE_REQUEST,

    //The last waapi import call suceeded, lparam is 0 - 100 integer for progress bar %
    IMPORT_SUCCESS,

    //The last waapi import call failed, lparam is 0 - 100 integer for progress bar %
    IMPORT_FAIL
};

enum PROGRESS_WINDOW_WPARAM : WPARAM
{
    EXIT,

    //lparm is 0 - 100 int for progress ID
    UPDATE_PROGRESS_SUCCESS,

    //lparm is 0 - 100 int for progress ID 
    UPDATE_PROGRESS_FAIL,
};


HWND OpenTransferWindow();

class WAAPITransfer;
void OpenProgressWindow(HWND parent, WAAPITransfer *parentTransfer);
void OpenImportSettingsWindow(HWND parent, WAAPITransfer *parentTransfer);


int CALLBACK WindowCompareFunc(LPARAM item1, LPARAM item2, LPARAM columnId);
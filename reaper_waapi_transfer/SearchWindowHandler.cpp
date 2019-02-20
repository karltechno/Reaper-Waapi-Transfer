#pragma once
#include "TransferWindowHandler.h"
#include "Reaper_WAAPI_Transfer.h"
#include "resource.h"
#include "TransferSearch.h"

HWND g_transferSearchWindow = 0;


INT_PTR WINAPI TransferSearchWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        g_transferSearchWindow = hwndDlg;
        TransferSearch *searchPtr = new TransferSearch(reinterpret_cast<WAAPITransfer*>(lParam), 
                                                       hwndDlg, 
                                                       IDC_REAPER_REGIONS, 
                                                       IDC_REGION_TRACKS_TO_RENDER);
      
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(searchPtr));

        ShowWindow(hwndDlg, SW_SHOW);
    } break;

    case WM_CLOSE:
    {
        DestroyWindow(hwndDlg);
    } break;

    case WM_DESTROY:
    {
        delete reinterpret_cast<TransferSearch*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
        g_transferSearchWindow = 0;
    } break;

    }
    return 0;
}

void OpenTransferSearchWindow(HWND parent, WAAPITransfer *parentTransfer)
{
    if (g_transferSearchWindow)
    {
        SetActiveWindow(g_transferSearchWindow);
    }
    else
    {
        CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_TRANSFER_SEARCH), parent, TransferSearchWindowProc,
                            reinterpret_cast<LPARAM>(parentTransfer));
    }
}

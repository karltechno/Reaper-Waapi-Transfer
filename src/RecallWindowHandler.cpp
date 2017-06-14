#include "RecallWindowHandler.h"
#include "Reaper_WAAPI_Transfer.h"
#include "WAAPIRecall.h"
#include "config.h"
#include "types.h"

#include "resource.h"


#define IDT_REFRESH_TIMER 1001

HWND g_recallWindow = 0;

INT_PTR WINAPI RecallWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        WAAPIRecall *recallPtr = new WAAPIRecall(hwndDlg, IDC_RECALL_LIST, IDC_STATUS);
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)recallPtr);
        g_recallWindow = hwndDlg;
        recallPtr->Connect();
        recallPtr->SetupWindow();
        recallPtr->UpdateWwiseObjects();
        ShowWindow(hwndDlg, SW_SHOW);
        SetTimer(hwndDlg, IDT_REFRESH_TIMER, 500, static_cast<TIMERPROC>(nullptr));
    } break;

    case WM_COMMAND:
    {
        WAAPIRecall *recallPtr = reinterpret_cast<WAAPIRecall*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
        switch (wParam)
        {
        case IDC_RECALL_BUTTON:
        {
            recallPtr->OpenSelectedProject();
        } break;

        case IDC_WAAPI_RECONNECT:
        {
            recallPtr->Connect();
        } break;

        }
    } break;

    case WM_TIMER:
    {
        switch (wParam)
        {
        case IDT_REFRESH_TIMER:
        {
            WAAPIRecall *recallPtr = reinterpret_cast<WAAPIRecall*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
            recallPtr->UpdateWwiseObjects();
        } break;

        default:
        {

        } break;

        }
    }

    case WM_NOTIFY:
    {
        switch (wParam)
        {
        //TODO: looks like this should only be done when using a virtual list view, change this...

        //recall list, need to set column subitems here
        //https://msdn.microsoft.com/en-us/library/windows/desktop/hh298346(v=vs.85).aspx
        case IDC_RECALL_LIST:
        {
            switch (((LPNMHDR)lParam)->code)
            {
                //switch case for subitem (column)
            case LVN_GETDISPINFO:
            {
                NMLVDISPINFO* plvdi = reinterpret_cast<NMLVDISPINFO*>(lParam);
                WAAPIRecall *recallPtr = reinterpret_cast<WAAPIRecall*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
                uint32 mappedId = ListView_MapIndexToID(recallPtr->GetRecallListHWND(), plvdi->item.iItem);

                switch (plvdi->item.iSubItem)
                {
                case WAAPIRecall::SubitemID::WwiseObjectName:
                {    
                    strcpy(plvdi->item.pszText, recallPtr->GetRecallItem(mappedId).wwiseName.c_str());
                } break;
                case WAAPIRecall::SubitemID::ReaProjectName:
                {
                    auto projectPath = recallPtr->GetRecallItem(mappedId).projectPath;
                    const std::string filename = fs::path(projectPath).filename().generic_string();
                    strcpy(plvdi->item.pszText, filename.c_str());

                } break;
                case WAAPIRecall::SubitemID::ReaProjectPath:
                {
                    const char* recallpath = recallPtr->GetRecallItem(mappedId).projectPath.c_str();
                    strcpy(plvdi->item.pszText, recallpath);
                } break;
                } //switch (plvdi->item.iSubItem)

            } break;

            default:
                break;
            } //switch (((LPNMHDR)lParam)->code)


        } break;
        } //switch (wParam)
    } break;

    case WM_DESTROY:
    {
        delete reinterpret_cast<WAAPIRecall*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
    } break;

    case WM_CLOSE:
    {
        DestroyWindow(hwndDlg);
        g_recallWindow = 0;
    } break;

    default:
    {
    } break;

    } //switch (uMsg)
    return 0;
}

void OpenRecallWindow()
{
    if (g_recallWindow)
    {
        SetActiveWindow(g_recallWindow);
    }
    else
    {
        CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_RECALL), g_parentWindow, RecallWindowProc);
    }
}
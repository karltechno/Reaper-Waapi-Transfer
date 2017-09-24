#include <Windows.h>
#include <Windowsx.h>

#include "resource.h"

#include "reaper_plugin_functions.h"

#include "WAAPITransfer.h"
#include "TransferWindowHandler.h"
#include "Reaper_WAAPI_Transfer.h"
#include "RenderQueueReader.h"
#include "WAAPIHelpers.h"
#include "SearchWindowHandler.h"
#include "config.h"

#include "types.h"

HWND g_transferWindow = 0;
HWND g_importSettingsWindow = 0;

//forward declerations
INT_PTR WINAPI ImportSettingsWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR WINAPI AboutWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR WINAPI TransferWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);


INT_PTR WINAPI AboutWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        char strbuff[256];
        std::string str("WAAPI Transfer Version: " 
                        + std::to_string(WT_VERSION_MAJOR) + '.' 
                        + std::to_string(WT_VERSION_MINOR) + '.' 
                        + std::to_string(WT_VERSION_INCREMENTAL));

        strcpy(strbuff, str.c_str());

        SetWindowText(GetDlgItem(hwndDlg, IDC_VERSION), strbuff);
        ShowWindow(hwndDlg, SW_SHOW);
    } break;

    case WM_CLOSE:
    {
        DestroyWindow(hwndDlg);
    } break;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////

const uint32 IDT_REFRESH_VIEW_TIMER = 1002;

#define START_REFRESH_TIMER(hwnd) SetTimer(hwnd, IDT_REFRESH_VIEW_TIMER, 1500, static_cast<TIMERPROC>(nullptr))
#define STOP_REFRESH_TIMER(hwnd) KillTimer(hwnd, IDT_REFRESH_VIEW_TIMER)
#define TRANSFER_HOTKEY_SELECT_ALL 1
INT_PTR WINAPI TransferWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        //register hotkey crtl+a for selecting all render items
        RegisterHotKey(hwndDlg, TRANSFER_HOTKEY_SELECT_ALL, MOD_CONTROL | MOD_NOREPEAT, 0x41);

        WAAPITransfer *transfer = new WAAPITransfer(hwndDlg, IDC_WWISEOBJECT_LIST, IDC_STATUS, IDC_TRANSFER_LIST);
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)transfer);
        g_transferWindow = hwndDlg;

		HWND button = GetDlgItem(hwndDlg, IDC_COPY_FILES_TO_ORIGINALS);
		if (transfer->ShouldCopyToOriginals())
		{
			Button_SetCheck(button, BST_CHECKED);
		}
		else
		{
			Button_SetCheck(button, BST_UNCHECKED);
		}

        transfer->SetupAndRecreateWindow();

        transfer->UpdateRenderQueue();

        START_REFRESH_TIMER(hwndDlg);

        ShowWindow(hwndDlg, SW_SHOW);
    } break;

    case WM_HOTKEY:
    {
        WAAPITransfer *transfer = reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
        switch (wParam)
        {

        case TRANSFER_HOTKEY_SELECT_ALL:
        {
            //select all listview items
            HWND listView = transfer->GetRenderViewHWND();
            int listItem = SendMessage(listView, LVM_GETNEXTITEM, -1, LVNI_ALL);
            while (listItem != -1)
            {
                ListView_SetItemState(listView, listItem, LVIS_SELECTED, LVIS_SELECTED);
                listItem = SendMessage(listView, LVM_GETNEXTITEM, listItem, LVNI_ALL);
            };

        } break;
        default:
        {
        } break;

        }

    } break;

    case WM_COMMAND:
    {
        WAAPITransfer *transfer = reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
        switch (wParam)
        {
        case IDC_WAAPI_RECONNECT:
        {
            transfer->Connect();
        } break;

        case IDC_BATCH_SELECT:
        { 
            OpenTransferSearchWindow(hwndDlg, transfer);
        } break;

        case IDC_ADD_SELECTED_WWISE:
        {
            transfer->AddSelectedWwiseObjects();
        } break;

        case IDC_REMOVE_SELECTED:
        {
            auto selected = ListView_GetNextItem(transfer->GetWwiseObjectListHWND(), -1, LVNI_SELECTED);
            if (selected != -1)
            {
                transfer->RemoveWwiseObject(ListView_MapIndexToID(transfer->GetWwiseObjectListHWND(), selected));
            }
        } break;

        case IDC_REMOVE_ALL:
        {
            transfer->RemoveAllWwiseObjects();
        } break;

        case IDC_RENDER:
        {
            transfer->RunRenderQueueAndImport();
        } break;

        case IDC_SET_WWISE_IMPORT_LOCATION:
        {
            auto selected = ListView_GetNextItem(transfer->GetWwiseObjectListHWND(), -1, LVNI_SELECTED);
            if (selected != -1)
            {
                transfer->SetSelectedRenderParents(ListView_MapIndexToID(transfer->GetWwiseObjectListHWND(), selected));
            }
        } break;
        
        case IDC_IMPORT_SETTINGS_BUTTON:
        {
            OpenImportSettingsWindow(hwndDlg, reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA)));
        } break;

		case IDC_COPY_FILES_TO_ORIGINALS:
		{
			LRESULT checked = Button_GetCheck(GetDlgItem(hwndDlg, IDC_COPY_FILES_TO_ORIGINALS));
			if (checked == BST_CHECKED)
			{
				transfer->SetShouldCopyToOriginals(true);
			}
			else if (checked == BST_UNCHECKED)
			{
				transfer->SetShouldCopyToOriginals(false);
			}

		} break;

        default:
        {
        } break;
        
        }
        
    } break;

    case WM_MENUSELECT:
    {
        auto uItem = (UINT)LOWORD(wParam);
        auto fuFlags = (UINT)HIWORD(wParam);
        switch (uItem)
        {
        case IDR_ABOUT_BUTTON:
        {
            if (fuFlags & MF_MOUSESELECT)
            {
                CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hwndDlg, AboutWindowProc);
            }
        } break;

        default:
        {
        } break;
        }

    } break;

    case WM_TIMER:
    {
        switch (wParam)
        {

        case IDT_REFRESH_VIEW_TIMER:
        {
            reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA))->UpdateRenderQueue();
        } break;

        default:
        {        
        } break;

        }
    } break;

    case WM_NOTIFY:
    {
        switch (wParam)
        {
        case LVN_COLUMNCLICK:
        {
            int x = 5;
        } break;

        default:
        {
        } break;

        }
    } break;

    //message from transfer thread 
    case WM_TRANSFER_THREAD_MSG:
    {
        WAAPITransfer *transferPtr = reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
        //the transfer thread spins up before the message box is open so it's possible the HWND isn't set yet
        HWND progressBarHWND = transferPtr->GetProgressWindowHWND();
        if (!progressBarHWND)
        {
            return 0;
        }

        switch (wParam)
        {

        case TRANSFER_THREAD_WPARAM::THREAD_EXIT_FAIL:
        {
            SendMessage(progressBarHWND, WM_PROGRESS_WINDOW_MSG, PROGRESS_WINDOW_WPARAM::EXIT, 0);
            MessageBox(hwndDlg, "Some files could not be imported into Wwise\n"
                       "The associated render queue projects have been re-created.",
                       "Waapi Error",
                       MB_OK | MB_ICONERROR);

            transferPtr->UpdateRenderQueue();
            START_REFRESH_TIMER(hwndDlg);
        } break;

        case TRANSFER_THREAD_WPARAM::THREAD_EXIT_SUCCESS:
        {
            SendMessage(progressBarHWND, WM_PROGRESS_WINDOW_MSG, PROGRESS_WINDOW_WPARAM::EXIT, 0);
            transferPtr->SetStatusText("Successfuly imported render queue.");
            
            transferPtr->UpdateRenderQueue();

            START_REFRESH_TIMER(hwndDlg);
        } break;

        case TRANSFER_THREAD_WPARAM::THREAD_EXIT_BY_USER:
        {
            SendMessage(progressBarHWND, WM_PROGRESS_WINDOW_MSG, PROGRESS_WINDOW_WPARAM::EXIT, 0);
            transferPtr->SetStatusText("Import cancelled by user.");

            transferPtr->UpdateRenderQueue();

            START_REFRESH_TIMER(hwndDlg);
        } break;

        case TRANSFER_THREAD_WPARAM::IMPORT_FAIL:
        {
            PostMessage(progressBarHWND, WM_PROGRESS_WINDOW_MSG, PROGRESS_WINDOW_WPARAM::UPDATE_PROGRESS_FAIL, lParam);
            transferPtr->SetStatusText("Some imports failed.");
        } break;

        case TRANSFER_THREAD_WPARAM::IMPORT_SUCCESS:
        {
            PostMessage(progressBarHWND, WM_PROGRESS_WINDOW_MSG, PROGRESS_WINDOW_WPARAM::UPDATE_PROGRESS_SUCCESS, lParam);
        } break;

        case TRANSFER_THREAD_WPARAM::LAUNCH_RENDER_QUEUE_REQUEST:
        {
            STOP_REFRESH_TIMER(hwndDlg);
            Main_OnCommand(41207, 1);
        } break;

        default:
        {
        } break;

        }
    } break;

#if 0
    case WM_NOTIFY:
    {
        switch (wParam)
        {
        //transfer list, need to set column subitems here
        //https://msdn.microsoft.com/en-us/library/windows/desktop/hh298346(v=vs.85).aspx
        case IDC_TRANSFER_LIST:
        {

            switch (((LPNMHDR)lParam)->code)
            {
                //switch case for subitem (column)

            case LVN_GETDISPINFO:
            {
                NMLVDISPINFO* plvdi = reinterpret_cast<NMLVDISPINFO*>(lParam);
                WAAPITransfer *transferPtr = reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
                uint32 mappedId = ListView_MapIndexToID(transferPtr->GetRenderViewHWND(), plvdi->item.iItem);
                RenderItem &item = transferPtr->GetRenderItemFromListviewId(mappedId);

                switch (plvdi->item.iSubItem)
                {
                case WAAPITransfer::RenderViewSubitemID::WwiseParent:
                {
                    strcpy(plvdi->item.pszText, item.wwiseParentName.c_str());
                } break;

                case WAAPITransfer::RenderViewSubitemID::WwiseImportObjectType:
                {
                    char *typeStr = item.isDialog ? "Dialog" : "SFX";
                    strcpy(plvdi->item.pszText, typeStr);
                } break;

                case WAAPITransfer::RenderViewSubitemID::WwiseLanguage:
                {
                    strcpy(plvdi->item.pszText, WwiseLanguages[item.wwiseLanguageIndex]);
                } break;

                }

            } break;

            default:
            {
            } break;

            }



        } break;
        }
    } break;
#endif

    case WM_DESTROY:
    {
        //deregister hotkey crtl+a
        UnregisterHotKey(hwndDlg, TRANSFER_HOTKEY_SELECT_ALL);

        delete reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));

        g_transferWindow = 0;
    } break;

    case WM_CLOSE:
    {
        DestroyWindow(hwndDlg);
    } break;

    default:
    {
    } break;

    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////

INT_PTR WINAPI ProgressWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
        reinterpret_cast<WAAPITransfer*>(lParam)->SetProgressWindowHWND(hwndDlg);
        ShowWindow(hwndDlg, SW_SHOW);
    } break;

    case WM_PROGRESS_WINDOW_MSG:
    {
        HWND progressBarHWND = GetDlgItem(hwndDlg, IDC_PROGRESS);

        switch (wParam)
        {
        case PROGRESS_WINDOW_WPARAM::UPDATE_PROGRESS_SUCCESS:
        {
            PostMessage(progressBarHWND, PBM_SETPOS, lParam, 0);
        } break;

        case PROGRESS_WINDOW_WPARAM::UPDATE_PROGRESS_FAIL:
        {
            PostMessage(progressBarHWND, PBM_SETPOS, lParam, 0);
        } break;

        case PROGRESS_WINDOW_WPARAM::EXIT:
        {
            reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA))->SetProgressWindowHWND(0);
            EndDialog(hwndDlg, 0);
        } break;

        default:
        {

        } break;

        }
    } break;

    case WM_COMMAND:
    {
        switch (wParam)
        {

        case IDCANCEL:
        {
            //stop waapi import thread
            int mboxReturn = MessageBox(hwndDlg, "Are you sure you want to cancel importing?\n"
                                        "This will not stop the render queue rendering!",
                                        "Cancel Transfer", MB_YESNO | MB_ICONEXCLAMATION);
            if (mboxReturn == IDYES)
            {
                reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA))->CancelTransferThread();
            }
        } break;

        default:
        {
        } break;

        } break;


    default:
    {
    } break;

    } break;

    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////

INT_PTR WINAPI ImportSettingsWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {

    case WM_INITDIALOG:
    {
        g_importSettingsWindow = hwndDlg;

        //WAAPITransfer pointer
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);

		WAAPITransfer *transferPtr = reinterpret_cast<WAAPITransfer*>(lParam);
		HWND comboBox = GetDlgItem(hwndDlg, IDC_ORIGINALS_TEXT);

		//add previous history strings
		for (std::string const& originalPath : transferPtr->s_originalPathHistory)
		{
			ComboBox_AddString(comboBox, const_cast<LPSTR>(originalPath.c_str()));
		}

        //add languages
        HWND comboBoxHWND = GetDlgItem(hwndDlg, IDC_LANGUAGE_DROPDOWN);
        for (uint32 i = 0; i < std::size(WwiseLanguages); ++i)
        {
            char strbuff[256];
            strcpy(strbuff, WwiseLanguages[i]);

            //make sure the combo box data is the index in wwiselanguages array
            int comboBoxIndex = ComboBox_AddString(comboBoxHWND, strbuff);
            ComboBox_SetItemData(comboBoxHWND, comboBoxIndex, i);
        }
        ComboBox_SetCurSel(comboBoxHWND, 0);


        //add import operations
        HWND importOperationHWND = GetDlgItem(hwndDlg, IDC_IMPORT_OPERATION_DROPDOWN);

        {
            char strbuff[64];
            char *str = "createNew";
            strcpy(strbuff, str);
            int comboBoxIndex = ComboBox_AddString(importOperationHWND, strbuff);
            ComboBox_SetItemData(importOperationHWND, comboBoxIndex, WAAPIImportOperation::createNew);
        }
        {
            char strbuff[64];
            char *str = "useExisting";
            strcpy(strbuff, str);
            int comboBoxIndex = ComboBox_AddString(importOperationHWND, strbuff);
            ComboBox_SetItemData(importOperationHWND, comboBoxIndex, WAAPIImportOperation::useExisting);
        }
        {
            char strbuff[64];
            char *str = "replaceExisting";
            strcpy(strbuff, str);
            int comboBoxIndex = ComboBox_AddString(importOperationHWND, strbuff);
            ComboBox_SetItemData(importOperationHWND, comboBoxIndex, WAAPIImportOperation::replaceExisting);
        }


        ComboBox_SetCurSel(importOperationHWND, WAAPITransfer::lastImportOperation);
        ShowWindow(hwndDlg, SW_SHOW);
    } break;

    case WM_COMMAND:
    {
        WAAPITransfer *transferPtr = reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
        switch (wParam)
        {
		case IDC_SET_ORIGINAL_BTN:
		{
			char strBuff[512];
			ComboBox_GetText(GetDlgItem(hwndDlg, IDC_ORIGINALS_TEXT), strBuff, 512);

			//Add to history if not there
			std::string pathStr(strBuff);

			//Wwise will complain and the plugin will recreate the reaper projects and exported sounds if the user provides an invalid path
			//but we can atleast check for partial correctness
			//https://msdn.microsoft.com/en-us/library/aa365247
			size_t errorCharPos = pathStr.find_first_of("\"<>:|?*");
			if (errorCharPos != std::string::npos)
			{
				char msgBoxError[256];
				sprintf_s(msgBoxError, "You entered an incorrect character in the originals path: %c", pathStr[errorCharPos]);

				MessageBox(hwndDlg, msgBoxError, "Waapi Error", MB_OK | MB_ICONERROR);
			}
			else
			{
				if (transferPtr->s_originalPathHistory.find(pathStr) == transferPtr->s_originalPathHistory.end())
				{
					transferPtr->s_originalPathHistory.insert(pathStr);
					ComboBox_AddString(GetDlgItem(hwndDlg, IDC_ORIGINALS_TEXT), strBuff);
				}

				transferPtr->ForEachSelectedRenderItem([transferPtr, &pathStr, strBuff](MappedListViewID mappedIdx, uint32 listViewIdx)
				{
					RenderItem &item = transferPtr->GetRenderItemFromListviewId(mappedIdx);
					item.wwiseOriginalsSubpath = pathStr;
					ListView_SetItemText(transferPtr->GetRenderViewHWND(), listViewIdx, WAAPITransfer::RenderViewSubitemID::WwiseOriginalsSubPath, (LPSTR)strBuff);
				});
			}

		} break;

        case IDC_IMPORT_SFX_BUTTON:
        {
            transferPtr->SetSelectedImportObjectType(ImportObjectType::SFX);
        } break;

        case IDC_IMPORT_DIALOG_BUTTON:
        {
            transferPtr->SetSelectedImportObjectType(ImportObjectType::Voice);
        } break;

        case IDC_LANGUAGE_BUTTON:
        {
           HWND comboBox = GetDlgItem(hwndDlg, IDC_LANGUAGE_DROPDOWN);
           transferPtr->SetSelectedImportObjectType(ImportObjectType::Voice);
           int index = ComboBox_GetItemData(comboBox, ComboBox_GetCurSel(comboBox));
           if (index != -1)
           {
               transferPtr->SetSelectedDialogLanguage(index);
           }
        } break;

        case IDC_IMPORT_OPERATION_BUTTON:
        {
            HWND comboBox = GetDlgItem(hwndDlg, IDC_IMPORT_OPERATION_DROPDOWN);
            int index = ComboBox_GetItemData(comboBox, ComboBox_GetCurSel(comboBox));
            transferPtr->SetSelectedImportOperation(static_cast<WAAPIImportOperation>(index));
            WAAPITransfer::lastImportOperation = static_cast<WAAPIImportOperation>(index);
        } break;

        }

    } break;

    case WM_CLOSE:
    {
        DestroyWindow(hwndDlg);
    } break;

    case WM_DESTROY:
    {
        g_importSettingsWindow = 0;
    } break;

    }

    return 0;
}


void OpenProgressWindow(HWND parent, WAAPITransfer *parentTransfer)
{
    //This dialog box is attached to a waapi transfer object so we pass it's pointer in as lparam
    DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_IMPORTING), parent, ProgressWindowProc, 
                   reinterpret_cast<LPARAM>(parentTransfer));
}

void OpenImportSettingsWindow(HWND parent, WAAPITransfer *parentTransfer)
{
    if (g_importSettingsWindow)
    {
        SetActiveWindow(g_importSettingsWindow);
    }
    else
    {
        CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_IMPORT_CONFIG), parent, ImportSettingsWindowProc, 
                          reinterpret_cast<LPARAM>(parentTransfer));
    }
}

HWND OpenTransferWindow()
{
    if (g_transferWindow)
    {
        return SetActiveWindow(g_transferWindow);
    }
    else
    {
        return CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_TRANSFER), g_parentWindow, TransferWindowProc);
    }
}

//used for sorting list view
int CALLBACK WindowCompareFunc(LPARAM item1, LPARAM item2, LPARAM columnId)
{
    switch (columnId)
    {

    case WAAPITransfer::RenderViewSubitemID::AudioFileName:
    {
        
    } break;

    case WAAPITransfer::RenderViewSubitemID::WwiseParent:
    {

    } break;

    case WAAPITransfer::RenderViewSubitemID::WwiseImportObjectType:
    {

    } break;

    case WAAPITransfer::RenderViewSubitemID::WwiseLanguage:
    {

    } break;

    case WAAPITransfer::RenderViewSubitemID::WaapiImportOperation:
    {

    } break;

    default:
    {
        assert(!"Unidentified column id.");
        return 0;
    }
    }
    return 0;
}

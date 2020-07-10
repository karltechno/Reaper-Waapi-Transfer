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
#include "ImGuiHandler.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "examples/imgui_impl_opengl3.h"

struct ImGuiTableHeader
{
	char const* const* colNames;
	int sortColIdx = 0;
	int numCols = 0;
	ImGuiDir sortColDir = ImGuiDir_Down;
};

struct TransferWindowState
{
	ImGuiTextFilter wwiseObjectFilter;
	ImGuiTableHeader renderQueueHeader;
	ImGuiTableHeader wwiseObjectHeader;
};

static TransferWindowState g_transferWindowState;

static HWND g_transferWindow = 0;
static HWND g_importSettingsWindow = 0;

static std::atomic<bool> g_isTransferThreadRunning = false;

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
			snprintf(strbuff, sizeof(strbuff), "WAAPI Transfer Version: %u.%u.%u", WT_VERSION_MAJOR, WT_VERSION_MINOR, WT_VERSION_INCREMENTAL);
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

static uint32 const s_contextMenuCreateNewId = 0xFE000000;
static uint32 const s_contextMenuReplaceExisting = 0xFE000000 | 0x1;
static uint32 const s_contextMenuUseExistingId = 0xFE000000 | 0x2;
static uint32 const s_contextMenuImportAsSFX = 0xFE000000 | 0x3;
static uint32 const s_contextMenuImportAsDialog = 0xFE000000 | 0x4;


LRESULT CALLBACK TransferWindow_ReaperKeyboardHook(int code, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT const* kb = (KBDLLHOOKSTRUCT *)lParam;
	
	if (!g_transferWindow)
	{
		return CallNextHookEx(0, code, wParam, lParam);
	}

	if (code == HC_ACTION && wParam == WM_KEYDOWN)
	{
		// win32 programming AT ITS BEST!
		if (IsChild(g_transferWindow, GetFocus()))
		{
			if (kb->vkCode == 0x41 && GetKeyState(VK_LCONTROL) & 0x8000) // 'A'
			{
				PostMessage(g_transferWindow, WM_TRANSFER_SELECT_ALL, 0, 0);
			}
			else if (kb->vkCode == VK_ESCAPE)
			{
				PostMessage(g_transferWindow, WM_CLOSE, 0, 0);
			}
		}
	}

	return CallNextHookEx(0, code, wParam, lParam);
}

INT_PTR WINAPI TransferWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_RBUTTONDOWN:
		{

		} break;

		case WM_INITDIALOG:
		{
			WAAPITransfer *transfer = nullptr;
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

			// transfer->SetupAndRecreateWindow();

			transfer->UpdateRenderQueue();

			START_REFRESH_TIMER(hwndDlg);

			ShowWindow(hwndDlg, SW_SHOW);
		} break;


		case WM_COMMAND:
		{
			WAAPITransfer *transfer = reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
			switch (wParam)
			{
				case s_contextMenuReplaceExisting:
				{
					transfer->SetSelectedImportOperation(WAAPIImportOperation::replaceExisting);
				} break;

				case s_contextMenuCreateNewId:
				{
					transfer->SetSelectedImportOperation(WAAPIImportOperation::createNew);
				} break;

				case s_contextMenuUseExistingId:
				{
					transfer->SetSelectedImportOperation(WAAPIImportOperation::useExisting);
				} break; 

				case s_contextMenuImportAsSFX:
				{
					transfer->SetSelectedImportObjectType(ImportObjectType::SFX);
				} break;

				case s_contextMenuImportAsDialog:
				{
					transfer->SetSelectedImportObjectType(ImportObjectType::Voice);
				} break;


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
						//transfer->SetSelectedRenderParents(ListView_MapIndexToID(transfer->GetWwiseObjectListHWND(), selected));
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

		case WM_TRANSFER_SELECT_ALL:
		{
			WAAPITransfer* transfer = reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
			//select all listview items
			HWND const listView = transfer->GetRenderViewHWND();
			LRESULT listItem = ListView_GetNextItem(listView, -1, LVNI_ALL);
			while (listItem != -1)
			{
				ListView_SetItemState(listView, listItem, LVIS_SELECTED, LVIS_SELECTED);
				listItem = ListView_GetNextItem(listView, listItem, LVNI_ALL);
			};
			return true;
		} break;

		case WM_CONTEXTMENU:
		{
			WAAPITransfer *transfer = reinterpret_cast<WAAPITransfer*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			RECT rect;
			GetWindowRect(transfer->GetRenderViewHWND(), &rect);

			if (rect.bottom >= yPos && rect.top <= yPos
				&& rect.left <= xPos && rect.right >= xPos)
			{
				HMENU hPopupMenu = CreatePopupMenu();
				InsertMenuA(hPopupMenu, -1, MF_BYPOSITION | MF_GRAYED | MF_STRING, 0, "Import Operation");
				InsertMenuA(hPopupMenu, -1, MF_BYPOSITION | MF_STRING, s_contextMenuCreateNewId, "createNew");
				InsertMenuA(hPopupMenu, -1, MF_BYPOSITION | MF_STRING, s_contextMenuReplaceExisting, "replaceExisting");
				InsertMenuA(hPopupMenu, -1, MF_BYPOSITION | MF_STRING, s_contextMenuUseExistingId, "useExisting");
				InsertMenuA(hPopupMenu, -1, MF_BYPOSITION | MF_GRAYED | MF_SEPARATOR, 0, nullptr);
				InsertMenuA(hPopupMenu, -1, MF_BYPOSITION | MF_GRAYED | MF_STRING, 0, "Import Type");
				InsertMenuA(hPopupMenu, -1, MF_BYPOSITION | MF_STRING, s_contextMenuImportAsSFX, "SFX");
				InsertMenuA(hPopupMenu, -1, MF_BYPOSITION | MF_STRING, s_contextMenuImportAsDialog, "Dialog");
				SetForegroundWindow(hwndDlg);
				TrackPopupMenu(hPopupMenu, TPM_TOPALIGN | TPM_LEFTALIGN, xPos, yPos, 0, hwndDlg, NULL);
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


		case WM_DESTROY:
		{
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

						//transferPtr->ForEachSelectedRenderItem([transferPtr, &pathStr, strBuff](MappedListViewID mappedIdx, uint32 listViewIdx)
						//{
						//	RenderItem &item = transferPtr->GetRenderItemFromListviewId(mappedIdx);
						//	item.wwiseOriginalsSubpath = pathStr;
						//	ListView_SetItemText(transferPtr->GetRenderViewHWND(), listViewIdx, WAAPITransfer::RenderViewSubitemID::WwiseOriginalsSubPath, (LPSTR)strBuff);
						//});
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
					HWND const comboBox = GetDlgItem(hwndDlg, IDC_LANGUAGE_DROPDOWN);
					transferPtr->SetSelectedImportObjectType(ImportObjectType::Voice);
					LRESULT const index = ComboBox_GetItemData(comboBox, ComboBox_GetCurSel(comboBox));
					if (index != -1)
					{
						transferPtr->SetSelectedDialogLanguage((int)index);
					}
				} break;

				case IDC_IMPORT_OPERATION_BUTTON:
				{
					HWND comboBox = GetDlgItem(hwndDlg, IDC_IMPORT_OPERATION_DROPDOWN);
					LRESULT const index = ComboBox_GetItemData(comboBox, ComboBox_GetCurSel(comboBox));
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

static void DoMenuBar(WAAPITransfer& transfer)
{
	ImGui::BeginMenuBar();

	if (transfer.m_connectionStatus)
	{
		ImGui::Text("Connected: %s", transfer.m_connectedWwiseVersion.c_str());
	}
	else
	{
		ImGui::Text("Not Connected");
	}

	if (ImGui::Button("Reconnect"))
	{
		transfer.Connect();
	}

	if (ImGui::Button("About"))
	{
		ImGui::OpenPopup("AboutPopup");
	}

	if (ImGui::BeginPopup("AboutPopup"))
	{
		ImGui::Text("WAAPI Transfer Version: %u.%u.%u", WT_VERSION_MAJOR, WT_VERSION_MINOR, WT_VERSION_INCREMENTAL);
		ImGui::EndPopup();
	}

	ImGui::EndMenuBar();
}


void DoImGuiTableHeader(ImGuiTableHeader& hdr)
{
	for (int i = 0; i < hdr.numCols; ++i)
	{
		ImGuiDir dir = hdr.sortColIdx == i ? hdr.sortColDir : ImGuiDir_Right;

		bool arrowClicked = ImGui::ArrowButton(hdr.colNames[i], dir); ImGui::SameLine(); ImGui::Text(hdr.colNames[i]);
		ImGui::NextColumn();
		if (arrowClicked)
		{
			if (hdr.sortColIdx == i)
			{
				hdr.sortColDir = hdr.sortColDir == ImGuiDir_Down ? ImGuiDir_Up : ImGuiDir_Down;
			}

			hdr.sortColIdx = i;
		}
	}
}

std::vector<WwiseObject*> GetSortedWwiseObjects(WAAPITransfer& transfer, ImGuiTableHeader& hdr, ImGuiTextFilter* filter = nullptr)
{
	std::vector<WwiseObject*> sortedObjects;
	sortedObjects.reserve(transfer.s_activeWwiseObjects.size());

	for (auto it = transfer.s_activeWwiseObjects.begin(); it != transfer.s_activeWwiseObjects.end(); ++it)
	{
		if (!filter || filter->PassFilter(it->second.name.c_str()))
		{
			sortedObjects.push_back(&it->second);
		}
	}

	switch (hdr.sortColIdx)
	{
		case 0: // Name
		{
			std::stable_sort(sortedObjects.begin(), sortedObjects.end(), [](WwiseObject* lhs, WwiseObject* rhs) { return lhs->name < rhs->name; });
		} break;

		case 1: // Path
		{
			std::stable_sort(sortedObjects.begin(), sortedObjects.end(), [](WwiseObject* lhs, WwiseObject* rhs) { return lhs->path < rhs->path; });
		} break;

		case 2: // Type
		{
			std::stable_sort(sortedObjects.begin(), sortedObjects.end(), [](WwiseObject* lhs, WwiseObject* rhs) { return lhs->type < rhs->type; });
		} break;
	}

	// TODO
	if (hdr.sortColDir == ImGuiDir_Up)
	{
		std::reverse(sortedObjects.begin(), sortedObjects.end());
	}

	return sortedObjects;
}

void DoWwiseObjectWindow(WAAPITransfer& transfer)
{
	if (ImGui::BeginChild("WwiseObjects"))
	{
		ImGuiTableHeader& hdr = g_transferWindowState.wwiseObjectHeader;

		assert(hdr.numCols == 3);
		ImGui::Columns(3);
		DoImGuiTableHeader(hdr);

		std::vector<WwiseObject*> sortedObjects = GetSortedWwiseObjects(transfer, hdr, nullptr);

		for (std::vector<WwiseObject*>::iterator it = sortedObjects.begin(); it != sortedObjects.end(); ++it)
		{
			WwiseObject* obj = (*it);
			ImGui::Selectable(obj->name.c_str(), &obj->isSelectedImGui, ImGuiSelectableFlags_SpanAllColumns);
			ImGui::NextColumn();
			ImGui::Text("%s", obj->path.c_str());
			ImGui::NextColumn();
			ImGui::Text("%s", obj->type.c_str());
			ImGui::NextColumn();
		}

		ImGui::Columns();
	}

	if (ImGui::Button("Add Selected Wwise Objects"))
	{
		transfer.AddSelectedWwiseObjects();
	}

	ImGui::EndChild();
}

static void DoWwiseParentPopup(WAAPITransfer& transfer)
{
	ImGuiTableHeader& hdr = g_transferWindowState.wwiseObjectHeader;

	assert(hdr.numCols == 3);
	g_transferWindowState.wwiseObjectFilter.Draw();
	ImGui::Columns(3);
	DoImGuiTableHeader(hdr);

	std::vector<WwiseObject*> sortedObjects = GetSortedWwiseObjects(transfer, hdr, &g_transferWindowState.wwiseObjectFilter);

	for (std::vector<WwiseObject*>::iterator it = sortedObjects.begin(); it != sortedObjects.end(); ++it)
	{
		WwiseObject* obj = (*it);
		
		if (ImGui::Selectable(obj->name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
		{
			transfer.SetSelectedRenderParents(obj->guid);
		}
		ImGui::NextColumn();
		ImGui::Text("%s", obj->path.c_str());
		ImGui::NextColumn();
		ImGui::Text("%s", obj->type.c_str());
		ImGui::NextColumn();
	}

	ImGui::Columns();
}


static void DoRenderQueueWindow(WAAPITransfer& transfer)
{
	if (ImGui::BeginChild("RenderQueue", ImVec2(0.0f, -25.0f)))
	{
		ImGuiTableHeader& hdr = g_transferWindowState.renderQueueHeader;

		ImGuiIO const& io = ImGui::GetIO();

		assert(hdr.numCols == 5);
		ImGui::Columns(5);
		DoImGuiTableHeader(hdr);

		std::vector<RenderItem*> sortedObjects;
		sortedObjects.reserve(transfer.s_renderQueueItems.size());
		for (auto it = transfer.s_renderQueueItems.begin(); it != transfer.s_renderQueueItems.end(); ++it)
		{
			sortedObjects.push_back(&it->second.first);
		}

		bool(*sortFn)(RenderItem*, RenderItem*) = nullptr;
		bool const reverse = hdr.sortColDir == ImGuiDir_Up;
		switch (hdr.sortColIdx)
		{
			case 0: // File
			{
				if (reverse)
				{
					sortFn = [](RenderItem* lhs, RenderItem* rhs) { return (lhs->outputFileName > rhs->outputFileName); };
				}
				else
				{
					sortFn = [](RenderItem* lhs, RenderItem* rhs) { return (lhs->outputFileName < rhs->outputFileName); };
				}
			} break;

			case 1: // Path
			{
				if (reverse)
				{
					sortFn = [](RenderItem* lhs, RenderItem* rhs) { return (lhs->wwiseParentName > rhs->wwiseParentName); };
				}
				else
				{
					sortFn = [](RenderItem* lhs, RenderItem* rhs) { return (lhs->wwiseParentName < rhs->wwiseParentName); };
				}
			} break;

			case 2: // Import Type
			{
				if (reverse)
				{
					sortFn = [](RenderItem* lhs, RenderItem* rhs) { return uint32(lhs->importObjectType) > uint32(rhs->importObjectType); };
				}
				else
				{
					sortFn = [](RenderItem* lhs, RenderItem* rhs) { return uint32(lhs->importObjectType) < uint32(rhs->importObjectType); };
				}
			} break;

			case 3: // Language
			{
				if (reverse)
				{
					sortFn = [](RenderItem* lhs, RenderItem* rhs) { return strcmp(WwiseLanguages[lhs->wwiseLanguageIndex], WwiseLanguages[rhs->wwiseLanguageIndex]) > 0;  };
				}
				else
				{
					sortFn = [](RenderItem* lhs, RenderItem* rhs) { return strcmp(WwiseLanguages[lhs->wwiseLanguageIndex], WwiseLanguages[rhs->wwiseLanguageIndex]) < 0;  };
				}
			} break;

			case 4: // Import op
			{
				if (reverse)
				{
					sortFn = [](RenderItem* lhs, RenderItem* rhs) { return uint32(lhs->importOperation) > uint32(rhs->importOperation); };
				}
				else
				{
					sortFn = [](RenderItem* lhs, RenderItem* rhs) { return uint32(lhs->importOperation) < uint32(rhs->importOperation); };
				}
			} break;
		}

		std::stable_sort(sortedObjects.begin(), sortedObjects.end(), sortFn);

		for (RenderItem* obj : sortedObjects)
		{
			bool selected = ImGui::Selectable(obj->outputFileName.c_str(), obj->isSelectedImGui, ImGuiSelectableFlags_SpanAllColumns);
			bool rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

			ImGui::OpenPopupContextItem("RenderQueuePopup", ImGuiPopupFlags_MouseButtonRight);

			if(selected)
			{
				if (io.KeyCtrl)
				{
					obj->isSelectedImGui = !obj->isSelectedImGui;
				}
				else if (io.KeyShift)
				{
					// Get last focused and try to select all in between
				}
				else
				{
					// Clear other selected
					for (RenderItem* obj2 : sortedObjects)
					{
						obj2->isSelectedImGui = false;
					}
					obj->isSelectedImGui = true;
				}
			}

			if(rightClicked && !obj->isSelectedImGui && !io.KeyCtrl)
			{
				// Clear other selected
				for (RenderItem* obj2 : sortedObjects)
				{
					obj2->isSelectedImGui = false;
				}
				obj->isSelectedImGui = true;
			}

			ImGui::NextColumn();
			ImGui::Text("%s", obj->wwiseParentName.c_str());
			ImGui::NextColumn();
			ImGui::Text("%s", GetTextForImportObject(obj->importObjectType));
			ImGui::NextColumn();
			ImGui::Text("%s", WwiseLanguages[obj->wwiseLanguageIndex]);
			ImGui::NextColumn();
			ImGui::Text("%s", GetImportOperationString(obj->importOperation));
			ImGui::NextColumn();
		}

		ImGui::Columns();

		bool openSetWwiseParentPopup = false;

		if (ImGui::BeginPopupContextItem("RenderQueuePopup"))
		{
			if (ImGui::MenuItem("Set Wwise Parent"))
			{
				openSetWwiseParentPopup = true;
			}

			if (ImGui::BeginMenu("Import Operation"))
			{
				if (ImGui::MenuItem("Create New"))
				{
					transfer.SetSelectedImportOperation(WAAPIImportOperation::createNew);
				}

				if (ImGui::MenuItem("Replace Existing"))
				{
					transfer.SetSelectedImportOperation(WAAPIImportOperation::replaceExisting);
				}

				if (ImGui::MenuItem("Use Existing"))
				{
					transfer.SetSelectedImportOperation(WAAPIImportOperation::useExisting);
				}

				ImGui::EndMenu();
			}


			if (ImGui::BeginMenu("Import Type"))
			{
				if (ImGui::MenuItem("Dialog"))
				{
					transfer.SetSelectedImportObjectType(ImportObjectType::Voice);
				}

				if (ImGui::MenuItem("SFX"))
				{
					transfer.SetSelectedImportObjectType(ImportObjectType::SFX);
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Language"))
			{
				uint32 constexpr numWwiseLanguages = sizeof(WwiseLanguages) / sizeof(*WwiseLanguages);
				bool selectedLangTable[numWwiseLanguages] = {};
				transfer.ForEachSelectedRenderItem([&selectedLangTable](RenderItem& renderItem) { selectedLangTable[renderItem.wwiseLanguageIndex] = true; });
				
				for (uint32 wwiseLangIdx = 0; wwiseLangIdx < numWwiseLanguages; ++wwiseLangIdx)
				{
					if (ImGui::MenuItem(WwiseLanguages[wwiseLangIdx], nullptr, selectedLangTable[wwiseLangIdx]))
					{
						transfer.SetSelectedDialogLanguage(wwiseLangIdx);
					}
				}
				ImGui::EndMenu();
			}

			ImGui::EndPopup();
		}

		if (openSetWwiseParentPopup)
		{
			ImGui::OpenPopup("SetWwiseParentPopup");
		}

		if (ImGui::BeginPopup("SetWwiseParentPopup"))
		{
			DoWwiseParentPopup(transfer);
			ImGui::EndPopup();
		}
	}
	ImGui::EndChild();

	if (ImGui::Button("Submit Render Queue"))
	{
		transfer.RunRenderQueueAndImport();
	}
}

static void TransferThreadFn()
{
	if (!ImGuiHandler::InitWindow("Reaper Waapi Transfer", 640, 480))
	{
		// TODO: Error
		return;
	}
	
	WAAPITransfer transfer;

	{
		static char const* const wwiseObjColNames[] = { "Name", "Path", "Type" };
		g_transferWindowState.wwiseObjectHeader.colNames = wwiseObjColNames;
		g_transferWindowState.wwiseObjectHeader.numCols = int(sizeof(wwiseObjColNames) / sizeof(*wwiseObjColNames));
	}

	{
		static char const* const renderItemColNames[] = { "File", "Wwise Parent", "Import Type", "Language", "Import Op" };
		g_transferWindowState.renderQueueHeader.colNames = renderItemColNames;
		g_transferWindowState.renderQueueHeader.numCols = int(sizeof(renderItemColNames) / sizeof(*renderItemColNames));
	}

	while (!glfwWindowShouldClose(ImGuiHandler::GetGLFWWindow()))
	{
		// TODO: Shoddy thread safety.
		// TODO: Socket calls off GUI thread.
		if (!transfer.m_isTransferring.load(std::memory_order_relaxed))
		{
			transfer.UpdateRenderQueue();
		}

		ImGuiHandler::BeginFrame();

		GLFWwindow* glfwWindow = ImGuiHandler::GetGLFWWindow();

		unsigned const windowFlags = ImGuiWindowFlags_NoCollapse
			| ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_MenuBar;

		ImGuiIO& io = ImGui::GetIO();
		
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);	
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::Begin("ReaperWaapiTransfer", nullptr, windowFlags);

		if (ImGui::BeginPopupModal("Transferring", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
		{
			int percent = transfer.m_transferProgress.load(std::memory_order_relaxed);
			ImGui::Text("Transferring, please wait (%d%%)...", percent);
			ImGui::ProgressBar(percent / 100.0f);
			if (ImGui::Button("Cancel"))
			{
				transfer.CancelTransferThread();
			}
			if (!transfer.m_isTransferring.load(std::memory_order_relaxed))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		DoMenuBar(transfer);
		if (ImGui::BeginTabBar("Tabs"))
		{
			if (ImGui::BeginTabItem("Render Queue"))
			{
				DoRenderQueueWindow(transfer);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Wwise Objects"))
			{
				DoWwiseObjectWindow(transfer);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		if (transfer.m_isTransferring.load(std::memory_order_relaxed))
		{
            ImGui::OpenPopup("Transferring");
		}

		ImGui::End();
		ImGui::PopStyleVar(2);

		ImGuiHandler::EndFrame();

		// TODO
		Sleep(33);
	}

	ImGuiHandler::ShutdownWindow();

	g_isTransferThreadRunning.store(false);
}

void OpenTransferWindow()
{
	// TODO: Currently races with shutdown.
	if (g_isTransferThreadRunning)
	{
		ImGuiHandler::BringToForeground();
		return;
	}

	g_isTransferThreadRunning.store(true);
	std::thread t(TransferThreadFn);
	t.detach();
}
